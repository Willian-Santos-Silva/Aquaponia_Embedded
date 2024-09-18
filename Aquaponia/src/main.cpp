#include "Aquarium/Aplicacoes.h"
#include "Aquarium/Routines.h"

#include "Aquarium/SetupDevice.h"
#include "Aquarium/Aquarium.h"
#include "Aquarium/AquariumServices.h"



#include <time.h>

#include "Clock/Clock.h"
#include "Base/memory.h"
#include "Base/config.h"

#include "rtos/TasksManagement.h"


#include "Bluetooth/BluetoothCallback.h"
#include "Bluetooth/BluetoothService.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>




BLEServer *pServer = NULL;
BLEService *pService;
BLEService *pServiceRoutinas;

BLECharacteristic *bleConfigurationCharacteristic,
                  *blePumpCharacteristic,
                  *bleSystemInformationCharacteristic,
                  *bleRTCCharacteristic,
                  *bleRoutinesUpdateCharacteristic,
                  *bleRoutinesGetCharacteristic,
                  *bleHistoricoApplyCharacteristic,
                  *bleHistoricoPhCharacteristic,
                  *bleHistoricoTempCharacteristic,
                  *bleRoutinesDeleteCharacteristic,
                  *bleRoutinesCreateCharacteristic;

BluetoothCallback bleConfigurationCallback,
                  blePumpCallback,
                  bleSystemInformationCallback,
                  bleRTCCallback,
                  bleHistoricoPhGetCallback,
                  bleHistoricoApplyGetCallback,
                  bleHistoricoTempCallback,
                  bleRoutinesUpdateCallback,
                  bleRoutinesDeleteCallback,
                  bleRoutinesGetCallback,
                  bleRoutinesCreateCallback;



Clock clockUTC;

Memory memory;

Aquarium aquarium(&memory);
SetupDevice aquariumSetupDevice(&aquarium, &memory);
AquariumServices aquariumServices(&aquarium, &aquariumSetupDevice);


using namespace std;


// ============================================================================================
//                                      TASKS
// ============================================================================================

TaskWrapper taskTemperatureControl;
TaskWrapper taskSendInfo;
TaskWrapper taskWaterPump;
TaskWrapper taskOneWire;
TaskWrapper taskScanWiFiDevices;
TaskWrapper taskPeristaultic;
TaskWrapper taskSaveLeitura;
SemaphoreHandle_t isExecutingOneWire;

void TaskSaveLeitura(){
  while(true){
    // try {
    //   vector<historicoTemperatura> lht = aquariumSetupDevice.read<historicoTemperatura>("/histTemp.bin");

    //   if(lht.size() >= 168)
    //       lht.erase(lht.begin());

    //   historicoTemperatura ht;
    //   ht.temperatura = aquarium.readTemperature();
    //   ht.time = clockUTC.getTimestamp();
    //   aquariumSetupDevice.write<historicoTemperatura>(lht, "/histTemp.bin");


    //   vector<historicoPh> lhph = aquariumSetupDevice.read<historicoPh>("/histTemp.bin");

    //   if(lhph.size() >- 168)
    //       lhph.erase(lhph.begin());

    //   historicoPh hph;
    //   hph.ph = aquarium.getPh();
    //   hph.time = clockUTC.getTimestamp();

    //   lhph.push_back(hph);

    //   aquariumSetupDevice.write<historicoPh>(lhph, "/histPh.bin");
    // }
    // catch (const std::exception& e)
    // {
    //     Serial.printf("erro: %s\r\n", e.what());
    // }
    vTaskDelay(3600000 / portTICK_PERIOD_MS);
  }
}


void TaskSendSystemInformation()
{
  while (true)
  {
    if(!xSemaphoreTake(isExecutingOneWire, portMAX_DELAY))
    {
      continue;
    }
    try
    {
      Serial.println("[LOG] ENVIO INFORMACAO SISTEMA");
      DynamicJsonDocument doc = aquariumServices.getSystemInformation();

      std::string resultString;
      serializeJson(doc, resultString);

      bleSystemInformationCallback.notify(bleSystemInformationCharacteristic, resultString.c_str());
      
      doc.clear();
    }
    catch (const std::exception& e)
    {
        Serial.printf("erro: %s\r\n", e.what());
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}
void TaskOneWireControl()
{
  while (true)
  {
    xSemaphoreGive(isExecutingOneWire);
    try
    {
      Serial.println("[LOG] LEITURA TERMOPAR");
      aquarium.updateTemperature();
    }
    catch (const std::exception& e)
    {
        Serial.printf("erro: %s\r\n", e.what());
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
void TaskAquariumTemperatureControl()
{
  float temperature;
  float flagTemeperature;
  float Kp = 2.0f, Ki = 5.0f, Kd = 1.0f;
  double integralError;
  double output;

  while (true)
  {
    if(!xSemaphoreTake(isExecutingOneWire, portMAX_DELAY))
    {
      continue;
    }
    
    temperature = aquarium.readTemperature();
    if (temperature == -127.0f)
    {
      aquarium.setStatusHeater(LOW);
      continue;
    }

    
    float goalTemperature = (aquarium.getMaxTemperature() - aquarium.getMinTemperature()) / 2 + aquarium.getMinTemperature();
    double erro = goalTemperature - temperature;
    integralError += erro;

    flagTemeperature = temperature;
    output = Kp * erro + Ki * integralError - Kd * (temperature - flagTemeperature);

    aquarium.setStatusHeater(output < 0);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
void TaskWaterPump()
{
  while (true)
  {
    try
    {
      std::string data;
      DynamicJsonDocument doc = aquariumServices.handlerWaterPump();
      serializeJson(doc, data);

      blePumpCallback.notify(blePumpCharacteristic, data.c_str());
    }
    catch (const std::exception& e)
    {
      Serial.printf("[erro] [WATER INFORMATION]: %s\r\n", e.what());
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
SemaphoreHandle_t xSemaphore = NULL;
void TaskPeristaultic()
{
  while (true)
  {
    try
    {
      aquariumServices.controlPeristaultic();
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    catch (const std::exception& e)
    {
      Serial.printf("[erro] [PERISTAULTIC INFORMATION]: %s\r\n", e.what());
    }

    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void startTasks(){
  isExecutingOneWire = xSemaphoreCreateBinary();
  taskTemperatureControl.begin(&TaskAquariumTemperatureControl, "TemperatureAquarium", 1300, 2);
  taskWaterPump.begin(&TaskWaterPump, "WaterPump",3000, 3);
  taskSendInfo.begin(&TaskSendSystemInformation, "SendInfo", 5000, 4);
  taskPeristaultic.begin(&TaskPeristaultic, "Peristautic", 5000, 1);
  taskOneWire.begin(&TaskOneWireControl, "OneWire", 1000, 2);
  taskSaveLeitura.begin(&TaskSaveLeitura, "SaveTemperatura", 5000, 3);
}



// // ============================================================================================
// //                                      ENDPOINTS
// // ============================================================================================

void  reset() {
  Serial.println("Reset");
  memory.clear();
  ESP.restart();
}

// Callback para conexão e desconexão
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        Serial.println("Cliente conectado");
    }

    void onDisconnect(BLEServer* pServer) {
        Serial.println("Cliente desconectado");
        pServer->getAdvertising()->start();
    }
};


DynamicJsonDocument getHistPhEndpoint(DynamicJsonDocument *doc)
{
  try {
    Serial.println("[LOG] GET PH");
    return aquariumServices.getHistPh();
  }
  catch(const std::exception& e)
  {
    Serial.printf("[callback] erro: %s\r\n", e.what());
    DynamicJsonDocument resp(300);
    return resp;
  }

}


DynamicJsonDocument getHistApplyEndpoint(DynamicJsonDocument *doc)
{
  try {
    Serial.println("[LOG] GET APPLY");
    return aquariumServices.getHistPh();
  }
  catch(const std::exception& e)
  {
    Serial.printf("[callback] erro: %s\r\n", e.what());
    DynamicJsonDocument resp(300);
    return resp;
  }

}


DynamicJsonDocument getHistTempEndpoint(DynamicJsonDocument *doc)
{
  try {
    Serial.println("[LOG] GET TEMPERATURA");
    return aquariumServices.getHistTemp();
  }
  catch(const std::exception& e)
  {
    Serial.printf("[callback] erro: %s\r\n", e.what());
    DynamicJsonDocument resp(300);
    return resp;
  }

}



DynamicJsonDocument SetRTC(DynamicJsonDocument *doc) {
  if (!doc->containsKey("rtc"))
  {
    throw std::runtime_error("Parametro fora de escopo");
  }
  long timestamp =(*doc)["rtc"].as<long>();
  struct tm timeinfo;
  localtime_r(&timestamp, &timeinfo);
  clockUTC.setRTC(&timeinfo);
  return *doc;
}


DynamicJsonDocument updateConfigurationEndpoint(DynamicJsonDocument *doc)
{
  Serial.println("[LOG] ATUALIZAR CONFIGURACAO");

  if (!doc->containsKey("min_temperature") || !doc->containsKey("max_temperature") || 
      !doc->containsKey("min_ph") || !doc->containsKey("max_ph") || 
      !doc->containsKey("dosagem"))
  {
    throw std::runtime_error("Parametro fora de escopo");
  }

  aquariumServices.updateConfiguration((*doc)["min_temperature"].as<int>(),
                                       (*doc)["max_temperature"].as<int>(),
                                       (*doc)["ph_max"].as<int>(),
                                       (*doc)["ph_min"].as<int>(),
                                       (*doc)["dosagem"].as<int>(),
                                       (*doc)["tempo_reaplicacao"].as<int>());
  return (*doc);
}
DynamicJsonDocument getConfigurationEndpoint(DynamicJsonDocument *doc)
{
  Serial.println("[LOG] GET CONFIGURACAO"); 

  return aquariumServices.getConfiguration();
}
DynamicJsonDocument getRoutinesEndpoint(DynamicJsonDocument *doc)
{
  try {
    Serial.println("[LOG] GET ROTINAS");
    return aquariumServices.getRoutines();
  }
  catch(const std::exception& e)
  {
    Serial.printf("[callback] erro: %s\r\n", e.what());
    DynamicJsonDocument resp(300);
    return resp;
  }

}
DynamicJsonDocument setRoutinesEndpoint(DynamicJsonDocument *doc)
{
  Serial.println("[LOG] ATUALIZAR ROTINAS");
  DynamicJsonDocument resp(5028);
  resp["status_code"] = 200;

  JsonObject jsonRoutine = doc->as<JsonObject>();
  routine r;
  strncpy(r.id, jsonRoutine["id"].as<const char*>(), 36);
  r.id[36] = '\0';
  
  JsonArray jsonWeekday = jsonRoutine["WeekDays"].as<JsonArray>();
  for (int i = 0; i < 7; i++) {
    r.weekday[i] = jsonWeekday[i];
  }

  uint16_t i = 0;
  for (JsonObject jsonHorario : jsonRoutine["horarios"].as<JsonArray>()) {
    horario h;
    h.start = jsonHorario["start"];
    h.end = jsonHorario["end"];
    r.horarios[i] = h;
    i++;
  }
  aquariumServices.setRoutines(r);
  bleRoutinesGetCharacteristic->notify();
  return resp;
}

DynamicJsonDocument createRoutinesEndpoint(DynamicJsonDocument *doc)
{
 
  Serial.println("[LOG] CRIAR ROTINA");
  DynamicJsonDocument resp(5028);
  resp["status_code"] = 200;

  JsonObject jsonRoutine = doc->as<JsonObject>();
  routine r;
  strncpy(r.id, jsonRoutine["id"].as<const char*>(), 36);
  r.id[36] = '\0';

  JsonArray jsonWeekday = jsonRoutine["WeekDays"].as<JsonArray>();
  for (int i = 0; i < 7; i++) {
      r.weekday[i] = jsonWeekday[i];
  }

  uint16_t i = 0;
  for (JsonObject jsonHorario : jsonRoutine["horarios"].as<JsonArray>()) {
      horario h;
      h.start = jsonHorario["start"];
      h.end = jsonHorario["end"];
      r.horarios[i] = h;
      i++;
  }
  aquariumServices.addRoutines(r);
  return resp;
}
DynamicJsonDocument deleteRoutinesEndpoint(DynamicJsonDocument *doc)
{
  Serial.println("[LOG] DELETAR ROTINA");
  DynamicJsonDocument resp(5028);
  resp["status_code"] = 200;

  JsonObject jsonRoutine = doc->as<JsonObject>();
  char id[37];
  strncpy(id, jsonRoutine["id"].as<const char*>(), 36);
  id[36] = '\0';

  aquariumServices.removeRoutine(id);
  return resp;
}


// ============================================================================================
//                                      START SERVICES
// ============================================================================================

void startBLE(){  
  BLEDevice::init("[AQP] AQUAPONIA");
  BLEDevice::setMTU(517);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pService = pServer->createService(SERVICE_UUID);
  
  bleSystemInformationCharacteristic = pService->createCharacteristic(CHARACTERISTIC_INFO_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  bleSystemInformationCharacteristic->setCallbacks(&bleSystemInformationCallback);
  bleSystemInformationCharacteristic->setValue("{}");
  
  // bleHistoricoApplyCharacteristic = pService->createCharacteristic(CHARACTERISTIC_GET_HIST_APL_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  // bleHistoricoApplyCharacteristic->setCallbacks(&bleHistoricoApplyGetCallback);
  // bleHistoricoApplyCharacteristic->setValue("{}");
  
  bleHistoricoTempCallback.onReadCallback = getHistTempEndpoint;
  bleHistoricoTempCharacteristic = pService->createCharacteristic(CHARACTERISTIC_GET_HIST_TEMP_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  bleHistoricoTempCharacteristic->setCallbacks(&bleHistoricoTempCallback);
  bleHistoricoTempCharacteristic->setValue("{}");
  
  bleHistoricoPhGetCallback.onReadCallback = getHistPhEndpoint;
  bleHistoricoPhCharacteristic = pService->createCharacteristic(CHARACTERISTIC_GET_HIST_PH_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  bleHistoricoPhCharacteristic->setCallbacks(&bleHistoricoPhGetCallback);
  bleHistoricoPhCharacteristic->setValue("{}");


  bleConfigurationCallback.onWriteCallback = updateConfigurationEndpoint;
  bleConfigurationCallback.onReadCallback = getConfigurationEndpoint;

  bleConfigurationCharacteristic = pService->createCharacteristic(CHARACTERISTIC_CONFIGURATION_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  bleConfigurationCharacteristic->setCallbacks(&bleConfigurationCallback);
  bleConfigurationCharacteristic->setValue("{}");

  bleRTCCallback.onWriteCallback = SetRTC;
  bleRTCCharacteristic = pService->createCharacteristic(CHARACTERISTIC_RTC_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  bleRTCCharacteristic->setCallbacks(&bleRTCCallback);
  bleRTCCharacteristic->setValue("{}");

  blePumpCharacteristic = pService->createCharacteristic(CHARACTERISTIC_PUMP_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  blePumpCharacteristic->setCallbacks(&blePumpCallback);
  blePumpCharacteristic->setValue("{}");


  pServiceRoutinas = pServer->createService(SERVICE_ROUTINES_UUID);

  bleRoutinesUpdateCallback.onWriteCallback = setRoutinesEndpoint;
  bleRoutinesUpdateCharacteristic = pServiceRoutinas->createCharacteristic(CHARACTERISTIC_UPDATE_ROUTINES_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  bleRoutinesUpdateCharacteristic->setCallbacks(&bleRoutinesUpdateCallback);
  bleRoutinesUpdateCharacteristic->setValue("{}");

  bleRoutinesGetCallback.onReadCallback = getRoutinesEndpoint;
  bleRoutinesGetCharacteristic = pServiceRoutinas->createCharacteristic(CHARACTERISTIC_GET_ROUTINES_UUID, BLECharacteristic::PROPERTY_READ |  BLECharacteristic::PROPERTY_NOTIFY);
  bleRoutinesGetCharacteristic->setCallbacks(&bleRoutinesGetCallback);
  bleRoutinesGetCharacteristic->setValue("{}");

  bleRoutinesDeleteCallback.onWriteCallback = deleteRoutinesEndpoint;
  bleRoutinesDeleteCharacteristic = pServiceRoutinas->createCharacteristic(CHARACTERISTIC_DELETE_ROUTINES_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  bleRoutinesDeleteCharacteristic->setCallbacks(&bleRoutinesDeleteCallback);
  bleRoutinesDeleteCharacteristic->setValue("{}");

  bleRoutinesCreateCallback.onWriteCallback = createRoutinesEndpoint;
  bleRoutinesCreateCharacteristic = pServiceRoutinas->createCharacteristic(CHARACTERISTIC_CREATE_ROUTINES_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  bleRoutinesCreateCharacteristic->setCallbacks(&bleRoutinesCreateCallback);
  bleRoutinesCreateCharacteristic->setValue("{}");



  pService->start();
  pServiceRoutinas->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->addServiceUUID(SERVICE_ROUTINES_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);

  pAdvertising->start();
}

void setup()
{
  Serial.begin(115200);

  
  aquariumSetupDevice.begin();
  attachInterrupt(PIN_RESET, reset, RISING);
  aquarium.begin();
  
  horario hh;
  hh.start = 0;
  hh.end = 1390;

  vector<routine> l;
  routine rr;
  rr.weekday[0] = true;
  rr.weekday[1] = true;
  rr.weekday[2] = true;
  rr.weekday[3] = true;
  rr.weekday[4] = true;
  rr.weekday[5] = true;
  rr.weekday[6] = true;

  strncpy(rr.id, "82495886-50a2-4336-9b1d-c00ed78c8978", 36);
  rr.id[36] = '\0';
  

  rr.horarios[0] = hh;

  l.push_back(rr);
  aquariumSetupDevice.write<routine>(l, "/rotinas.bin");



  DynamicJsonDocument doc(35000);
  JsonArray dataArray = doc.to<JsonArray>();

  vector<routine> data = aquariumSetupDevice.read<routine>("/rotinas.bin");    
  for (const auto& r : data) {
      JsonObject rotina = dataArray.createNestedObject();
      rotina["id"] = r.id;
      JsonArray weekdays = rotina.createNestedArray("WeekDays");
      for (size_t i = 0; i < sizeof(r.weekday) / sizeof(r.weekday[0]); ++i) {
          weekdays.add(r.weekday[i]);
      }

      JsonArray horarios = rotina.createNestedArray("horarios");
      for (const auto& h : r.horarios) {
          JsonObject horario = horarios.createNestedObject();
          horario["start"] = h.start;
          horario["end"] = h.end;
      }
  }

  data.clear();


  std::string dataStr;
  serializeJson(doc, dataStr);

  doc.clear();

  Serial.println(dataStr.c_str());

  // startBLE();
  // startTasks();


}

void loop()
{
}
