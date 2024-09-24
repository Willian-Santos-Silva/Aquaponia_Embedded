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

#include <NimBLEDevice.h>

vector<routine> listRotinas;

NimBLEServer *pServer = NULL;
NimBLEService *pService;
NimBLEService *pServiceRoutinas;

NimBLECharacteristic *bleConfigurationCharacteristic,
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

Aquarium aquarium;
SetupDevice aquariumSetupDevice(&aquarium);
AquariumServices aquariumServices(&aquarium);


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

void TaskSaveLeitura(){
  while(true){
    try {
      vector<historicoTemperatura> lht = aquariumSetupDevice.read<historicoTemperatura>("/histTemp.bin");
      if(lht.size() >= 168)
          lht.erase(lht.begin());

      historicoTemperatura ht;
      ht.temperatura = aquarium.readTemperature();
      ht.time = clockUTC.getTimestamp();

      lht.push_back(ht);

      aquariumSetupDevice.write<historicoTemperatura>(lht, "/histTemp.bin");

      vector<historicoPh> lhph = aquariumSetupDevice.read<historicoPh>("/histPh.bin");

      if(lhph.size() >= 168)
          lhph.erase(lhph.begin());

      historicoPh hph;
      hph.ph = aquarium.getPh();
      hph.time = clockUTC.getTimestamp();

      lhph.push_back(hph);

      aquariumSetupDevice.write<historicoPh>(lhph, "/histPh.bin");
    }
    catch (const std::exception& e)
    {
        log_e("erro: %s\r\n", e.what());
    }
    // vTaskDelay(3600000 / portTICK_PERIOD_MS);
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}


void TaskSendSystemInformation()
{
  while (true)
  {
    try
    {
      log_e("[LOG] ENVIO INFORMACAO SISTEMA");
      JsonDocument  doc = aquariumServices.getSystemInformation();

      String resultString;
      serializeJson(doc, resultString);
      serializeJson(doc, Serial);

      bleSystemInformationCallback.notify(bleSystemInformationCharacteristic, resultString);
      
      doc.clear();
    }
    catch (const std::exception& e)
    {
        log_e("erro: %s\r\n", e.what());
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void TaskAquariumTemperatureControl()
{
  double temperature;
  double flagTemeperature;
  double Kp = 2.0f, Ki = 5.0f, Kd = 1.0f;
  double integralError;
  double output;

  while (true)
  {    
    temperature = aquarium.readTemperature();
    if (temperature == -127.0f)
    {
      aquarium.setStatusHeater(LOW);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    
    double goalTemperature = (aquarium.getMaxTemperature() - aquarium.getMinTemperature()) / 2 + aquarium.getMinTemperature();
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
      String data;
      JsonDocument  doc = aquariumServices.handlerWaterPump();
      serializeJson(doc, data);

      blePumpCallback.notify(blePumpCharacteristic, data);
    }
    catch (const std::exception& e)
    {
      log_e("[erro] [WATER INFORMATION]: %s\r\n", e.what());
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

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
      log_e("[erro] [PERISTAULTIC INFORMATION]: %s\r\n", e.what());
    }

    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void startTasks(){
  taskTemperatureControl.begin(&TaskAquariumTemperatureControl, "TemperatureAquarium", 1300, 2);
  taskWaterPump.begin(&TaskWaterPump, "WaterPump",3000, 3);
  taskSendInfo.begin(&TaskSendSystemInformation, "SendInfo", 5000, 4);
  taskPeristaultic.begin(&TaskPeristaultic, "Peristautic", 5000, 1);
  taskSaveLeitura.begin(&TaskSaveLeitura, "SaveTemperatura", 5000, 3);
}



// // ============================================================================================
// //                                      ENDPOINTS
// // ============================================================================================

void  reset() {
  log_e("Reset");
  memory.clear();
  ESP.restart();
}

// Callback para conexão e desconexão
class MyServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        log_e("Cliente conectado");
    }

    void onDisconnect(BLEServer* pServer) {
        log_e("Cliente desconectado");
        pServer->getAdvertising()->start();
    }
};


JsonDocument  getHistPhEndpoint(JsonDocument  *doc)
{
  try {
    log_e("[LOG] GET PH");
    return aquariumServices.getHistPh();
  }
  catch(const std::exception& e)
  {
    log_e("[callback] erro: %s\r\n", e.what());
    JsonDocument  resp;
    return resp;
  }

}


JsonDocument  getHistApplyEndpoint(JsonDocument  *doc)
{
  try {
    log_e("[LOG] GET APPLY");
    return aquariumServices.getHistPh();
  }
  catch(const std::exception& e)
  {
    log_e("[callback] erro: %s\r\n", e.what());
    JsonDocument  resp;
    return resp;
  }

}


JsonDocument  getHistTempEndpoint(JsonDocument  *doc)
{
  try {
    log_e("[LOG] GET TEMPERATURA");
    return aquariumServices.getHistTemp();
  }
  catch(const std::exception& e)
  {
    log_e("[callback] erro: %s\r\n", e.what());
    JsonDocument  resp;
    return resp;
  }

}



JsonDocument  SetRTC(JsonDocument  *doc) {
  
  if (!(*doc)["rtc"].is<long>())
  {
    throw std::runtime_error("Parametro fora de escopo");
  }


  long timestamp =(*doc)["rtc"].as<long>();


  serializeJson(*doc, Serial);


  time_t timestamp_t = timestamp;
  tm * time = gmtime(&timestamp_t);
  clockUTC.setRTC(time);

  // struct tm timeinfo;
  // localtime_r(&timestamp, &timeinfo);
  // clockUTC.setRTC(&timeinfo);
  JsonDocument  resp;
  return resp;
}


JsonDocument  updateConfigurationEndpoint(JsonDocument  *doc)
{
  log_e("[LOG] ATUALIZAR CONFIGURACAO");

  if (!(*doc)["min_temperature"].is<int>() || !(*doc)["max_temperature"].is<int>() || 
      !(*doc)["min_ph"].is<int>() || !(*doc)["max_ph"].is<int>() || 
      !(*doc)["dosagem"].is<int>())
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
JsonDocument  getConfigurationEndpoint(JsonDocument  *doc)
{
  log_e("[LOG] GET CONFIGURACAO"); 

  return aquariumServices.getConfiguration();
}
JsonDocument  getRoutinesEndpoint(JsonDocument  *doc)
{
  try {
    log_e("[LOG] GET ROTINAS");
    return aquariumServices.getRoutines();
  }
  catch(const std::exception& e)
  {
    log_e("[callback] erro: %s\r\n", e.what());
    JsonDocument  resp;
    return resp;
  }

}
JsonDocument  setRoutinesEndpoint(JsonDocument  *doc)
{
  log_e("[LOG] ATUALIZAR ROTINAS");
  JsonDocument  resp;
  resp["status_code"] = 200;

  routine* r = new routine();
  strncpy(r->id, (*doc)["id"].as<const char*>(), 36);
  r->id[36] = '\0';
  
  JsonArray jsonWeekday = (*doc)["weekdays"].as<JsonArray>();
  for (int i = 0; i < 7; i++) {
    r->weekday[i] = jsonWeekday[i];
  }

  uint16_t i = 0;
  for (JsonObject jsonHorario : (*doc)["horarios"].as<JsonArray>()) {
    horario h;
    h.start = jsonHorario["start"];
    h.end = jsonHorario["end"];
    r->horarios[i] = h;
    i++;
  }
  aquariumServices.setRoutines(r);

  delete r;
  
  return resp;
}

JsonDocument  createRoutinesEndpoint(JsonDocument  *doc)
{
  log_e("[LOG] CRIAR ROTINA");

  routine* r = new routine();
  strncpy(r->id, (*doc)["id"].as<const char*>(), 36);
  r->id[36] = '\0';

  JsonArray jsonWeekday = (*doc)["weekdays"].as<JsonArray>();
  
  for (int i = 0; i < 7; i++) {
    r->weekday[i] = jsonWeekday[i];
  }
  
  ushort i = 0;
  for (JsonObject jsonHorario : (*doc)["horarios"].as<JsonArray>()) {
    horario h;
    h.start = jsonHorario["start"];
    h.end = jsonHorario["end"];
    r->horarios[i] = h;
    i++;
  }
  aquariumServices.addRoutines(r);
  
  delete r;
  log_e("SUCESSO");
  serializeJson((*doc), Serial);
  JsonDocument resp;
  resp["status_code"] = 200;

  return resp;
}
JsonDocument  deleteRoutinesEndpoint(JsonDocument  *doc)
{
  log_e("[LOG] DELETAR ROTINA");
  JsonDocument resp;
  resp["status_code"] = 200;

  char id[37];
  strncpy(id, (*doc)["id"].as<const char*>(), 36);
  id[36] = '\0';

  aquariumServices.removeRoutine(id);
  return resp;
}


// ============================================================================================
//                                      START SERVICES
// ============================================================================================

void startBLE(){  
  NimBLEDevice::init("[AQP] AQUAPONIA");
  NimBLEDevice::setMTU(517);


  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pService = pServer->createService(SERVICE_UUID);
  
  bleSystemInformationCharacteristic = pService->createCharacteristic(CHARACTERISTIC_INFO_UUID, NIMBLE_PROPERTY::NOTIFY);
  bleSystemInformationCharacteristic->setCallbacks(&bleSystemInformationCallback);
  bleSystemInformationCharacteristic->setValue("{}");
  
  // bleHistoricoApplyCharacteristic = pService->createCharacteristic(CHARACTERISTIC_GET_HIST_APL_UUID, NIMBLE_PROPERTY::NOTIFY);
  // bleHistoricoApplyCharacteristic->setCallbacks(&bleHistoricoApplyGetCallback);
  // bleHistoricoApplyCharacteristic->setValue("{}");
  
  bleHistoricoTempCallback.onReadCallback = getHistTempEndpoint;
  bleHistoricoTempCharacteristic = pService->createCharacteristic(CHARACTERISTIC_GET_HIST_TEMP_UUID,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |  NIMBLE_PROPERTY::NOTIFY);
  bleHistoricoTempCharacteristic->setCallbacks(&bleHistoricoTempCallback);
  bleHistoricoTempCharacteristic->setValue("{}");
  
  bleHistoricoPhGetCallback.onReadCallback = getHistPhEndpoint;
  bleHistoricoPhCharacteristic = pService->createCharacteristic(CHARACTERISTIC_GET_HIST_PH_UUID,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |  NIMBLE_PROPERTY::NOTIFY);
  bleHistoricoPhCharacteristic->setCallbacks(&bleHistoricoPhGetCallback);
  bleHistoricoPhCharacteristic->setValue("{}");


  bleConfigurationCallback.onWriteCallback = updateConfigurationEndpoint;
  bleConfigurationCallback.onReadCallback = getConfigurationEndpoint;

  bleConfigurationCharacteristic = pService->createCharacteristic(CHARACTERISTIC_CONFIGURATION_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  bleConfigurationCharacteristic->setCallbacks(&bleConfigurationCallback);
  bleConfigurationCharacteristic->setValue("{}");

  bleRTCCallback.onWriteCallback = SetRTC;
  bleRTCCharacteristic = pService->createCharacteristic(CHARACTERISTIC_RTC_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  bleRTCCharacteristic->setCallbacks(&bleRTCCallback);
  bleRTCCharacteristic->setValue("{}");

  blePumpCharacteristic = pService->createCharacteristic(CHARACTERISTIC_PUMP_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  blePumpCharacteristic->setCallbacks(&blePumpCallback);
  blePumpCharacteristic->setValue("{}");


  pServiceRoutinas = pServer->createService(SERVICE_ROUTINES_UUID);

  bleRoutinesUpdateCallback.onWriteCallback = setRoutinesEndpoint;
  bleRoutinesUpdateCharacteristic = pServiceRoutinas->createCharacteristic(CHARACTERISTIC_UPDATE_ROUTINES_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  bleRoutinesUpdateCharacteristic->setCallbacks(&bleRoutinesUpdateCallback);
  bleRoutinesUpdateCharacteristic->setValue("{}");

  bleRoutinesGetCallback.onReadCallback = getRoutinesEndpoint;
  bleRoutinesGetCharacteristic = pServiceRoutinas->createCharacteristic(CHARACTERISTIC_GET_ROUTINES_UUID, NIMBLE_PROPERTY::READ |  NIMBLE_PROPERTY::NOTIFY);
  bleRoutinesGetCharacteristic->setCallbacks(&bleRoutinesGetCallback);
  bleRoutinesGetCharacteristic->setValue("{}");

  bleRoutinesDeleteCallback.onWriteCallback = deleteRoutinesEndpoint;
  bleRoutinesDeleteCharacteristic = pServiceRoutinas->createCharacteristic(CHARACTERISTIC_DELETE_ROUTINES_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  bleRoutinesDeleteCharacteristic->setCallbacks(&bleRoutinesDeleteCallback);
  bleRoutinesDeleteCharacteristic->setValue("{}");

  bleRoutinesCreateCallback.onWriteCallback = createRoutinesEndpoint;
  bleRoutinesCreateCharacteristic = pServiceRoutinas->createCharacteristic(CHARACTERISTIC_CREATE_ROUTINES_UUID,  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  bleRoutinesCreateCharacteristic->setCallbacks(&bleRoutinesCreateCallback);
  bleRoutinesCreateCharacteristic->setValue("{}");



  pService->start();
  pServiceRoutinas->start();

  NimBLEAdvertising  *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->addServiceUUID(SERVICE_ROUTINES_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);

  pAdvertising->start();
}




void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  aquariumSetupDevice.begin();
  attachInterrupt(PIN_RESET, reset, RISING);
  aquarium.begin();

  startBLE();
  startTasks();
  
  while(true){
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void loop()
{
}
