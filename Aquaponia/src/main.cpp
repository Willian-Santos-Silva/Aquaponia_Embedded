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

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>



#include "nvs_flash.h"
#include "nvs.h"
#include "soc/rtc_wdt.h"
#include "esp_task_wdt.h"

uint32_t  remainingTime = 0; 
nvs_handle my_handle;

vector<routine> listRotinas;

BLEServer *pServer = NULL;
BLEService *pService,
           *pServiceRoutinas;

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

Aquarium aquarium;
SetupDevice aquariumSetupDevice(&aquarium);
AquariumServices aquariumServices(&aquarium);


using namespace std;


// ============================================================================================
//                                      TASKS
// ============================================================================================

TaskWrapper taskReadTemperature;
TaskWrapper taskTemperatureControl;
TaskWrapper taskSendInfo;
TaskWrapper taskWaterPump;
TaskWrapper taskOneWire;
TaskWrapper taskScanWiFiDevices;
TaskWrapper taskPeristaultic;
TaskWrapper taskSaveLeitura;


void TaskSaveLeitura(){
  uint32_t  lastTime = 0; 
  
  while(true){
    try {
      uint32_t currentTime = millis();
      uint32_t liquidTime = currentTime - lastTime + remainingTime;

      if (liquidTime < CYCLE_TIME_INFO_MS) {
        nvs_set_u32(my_handle, "remainingTime", liquidTime);
        vTaskDelay(pdMS_TO_TICKS(1.0));
        continue;
      }
      lastTime = currentTime;
      remainingTime = 0;

      nvs_set_u32(my_handle, "remainingTime", remainingTime);
      nvs_commit(my_handle);

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


      Serial.printf("[%s] ", clockUTC.getDateTimeString());
      Serial.printf("numero de leituras: %lu\r\n", (long)lht.size());
      Serial.printf("[%s] ", clockUTC.getDateTimeString());
      Serial.printf("numero de leituras: %lu\r\n\n", (long)lhph.size());
    }
    catch (const std::exception& e)
    {
        log_e("erro: %s\r\n", e.what());
    }
    vTaskDelay(pdMS_TO_TICKS(1.0));
  }
}


void TaskSendSystemInformation()
{
  while (true)
  {
    try
    {
      JsonDocument doc = aquariumServices.getSystemInformation();

      String resultString;
      serializeJson(doc, resultString);

      bleSystemInformationCallback.notify(bleSystemInformationCharacteristic, resultString);
      
      doc.clear();
    }
    catch (const std::exception& e)
    {
        log_e("erro: %s\r\n", e.what());
    }

    vTaskDelay(pdMS_TO_TICKS(5000.0));
  }
}

void TaskAquariumTemperatureControl()
{
  double temperature;
  // double flagTemeperature;
  // double Kp = 2.0f, Ki = 5.0f, Kd = 1.0f;
  // double integralError;
  // double output;

  while (true)
  {    
    temperature = aquarium.readTemperature();
    if (temperature == -127.0f)
    {
      aquarium.setStatusHeater(LOW);
      vTaskDelay(pdMS_TO_TICKS(1000.0));
      continue;
    }
    
    double goalTemperature = (aquarium.getMaxTemperature() + aquarium.getMinTemperature()) / 2;
    // double erro = goalTemperature - temperature;
    // integralError += erro;

    // flagTemeperature = temperature;
    // output = Kp * erro + Ki * integralError - Kd * (temperature - flagTemeperature);

    aquarium.setStatusHeater(temperature < goalTemperature);

    vTaskDelay(pdMS_TO_TICKS(1000.0));
  }
}

void TaskWaterPump()
{
  while (true)
  {
    try
    {
      String data;
      JsonDocument doc = aquariumServices.handlerWaterPump();
      serializeJson(doc, data);

      blePumpCallback.notify(blePumpCharacteristic, data);
    }
    catch (const std::exception& e)
    {
      log_e("[erro] [WATER INFORMATION]: %s\r\n", e.what());
    }
    vTaskDelay(pdMS_TO_TICKS(1000.0));
  }
}

void TaskPeristaultic()
{
  while (true)
  {
    try
    {
      vector<aplicacoes> aplicacaoList = aquariumSetupDevice.read<aplicacoes>("/pump.bin");

      aplicacoes applyRaiser = aquariumServices.applySolution(aplicacaoList, Aquarium::SOLUTION_RAISER);

      if(applyRaiser.ml > 0.0) {
          log_e("Dosagem Raiser: %lf", applyRaiser.ml);

          aplicacaoList.push_back(applyRaiser);

          if(aplicacaoList.size() > 10)
              aplicacaoList.erase(aplicacaoList.begin());

          aquariumSetupDevice.write<aplicacoes>(aplicacaoList, "/pump.bin");
      }


      aplicacoes applyLower = aquariumServices.applySolution(aplicacaoList, Aquarium::SOLUTION_LOWER);
      if(applyLower.ml > 0.0){
          log_e("Dosagem Lower: %lf", applyLower.ml);

          aplicacaoList.push_back(applyLower);
          
          if(aplicacaoList.size() > 10)
              aplicacaoList.erase(aplicacaoList.begin());
              
          aquariumSetupDevice.write<aplicacoes>(aplicacaoList, "/pump.bin");
      }

      aplicacaoList.clear();

    }
    catch (const std::exception& e)
    {
      log_e("[erro] [PERISTAULTIC INFORMATION]: %s\r\n", e.what());
    }

    vTaskDelay(pdMS_TO_TICKS(1000.0));
  }
}

void startTasks(){
  esp_task_wdt_init(10, true);
  // esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(0));
  // esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(1));

  taskPeristaultic.begin(&TaskPeristaultic, "Peristautic", 5000, 1); 
  taskTemperatureControl.begin(&TaskAquariumTemperatureControl, "TemperatureAquarium", 1300, 2);
  taskSaveLeitura.begin(&TaskSaveLeitura, "SaveTemperatura", 5000, 3);
  taskWaterPump.begin(&TaskWaterPump, "WaterPump", 3000, 4);
  taskSendInfo.begin(&TaskSendSystemInformation, "SendInfo", 3000, 5);
}



// ============================================================================================
//                                      ENDPOINTS
// ============================================================================================


JsonDocument getHistPhEndpoint(JsonDocument *doc)
{
  JsonDocument resp;
  resp = aquariumServices.getHistPh();
  return resp;
}


JsonDocument getHistApplyEndpoint(JsonDocument *doc)
{
  JsonDocument resp;
  resp = aquariumServices.getHistPh();
  return resp;
}

JsonDocument getHistTempEndpoint(JsonDocument *doc)
{
  JsonDocument resp;
  resp = aquariumServices.getHistTemp();
  return resp;
}

JsonDocument SetRTC(JsonDocument *doc) {
  JsonDocument resp;

  if (!(*doc)["rtc"].is<long>())
  {
    throw std::runtime_error("Parametro fora de escopo");
  }

  long timestamp =(*doc)["rtc"].as<long>();

  time_t time = timestamp;
  clockUTC.setRTC(&time);

  return resp;
}


JsonDocument updateConfigurationEndpoint(JsonDocument *doc)
{
  JsonDocument resp;
  if (!(*doc)["min_temperature"].is<int>() || !(*doc)["max_temperature"].is<int>() || 
      !(*doc)["min_ph"].is<int>() || !(*doc)["max_ph"].is<int>() || 
      !(*doc)["dosagem_solucao_acida"].is<int>() || !(*doc)["dosagem_solucao_base"].is<int>() || 
      !(*doc)["tempo_reaplicacao"].is<long>())
  {
    throw std::runtime_error("Parametro fora de escopo");
  }

  aquariumServices.updateConfiguration((*doc)["min_temperature"].as<int>(),
                                        (*doc)["max_temperature"].as<int>(),
                                        (*doc)["min_ph"].as<int>(),
                                        (*doc)["max_ph"].as<int>(),
                                        (*doc)["dosagem_solucao_acida"].as<int>(),
                                        (*doc)["dosagem_solucao_base"].as<int>(),
                                        (*doc)["tempo_reaplicacao"].as<long>());
  return resp;
}
JsonDocument getConfigurationEndpoint(JsonDocument *doc)
{
  JsonDocument resp;
  resp = aquariumServices.getConfiguration();
  return resp;
}

JsonDocument getRoutinesEndpoint(JsonDocument *doc)
{
  JsonDocument resp;
  resp = aquariumServices.getRoutines();
  return resp;
}

JsonDocument setRoutinesEndpoint(JsonDocument *doc)
{
  JsonDocument resp;
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

JsonDocument createRoutinesEndpoint(JsonDocument *doc)
{
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
  
  JsonDocument resp;
  resp["status_code"] = 200;

  return resp;
}
JsonDocument deleteRoutinesEndpoint(JsonDocument *doc)
{
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

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
    }
    void onDisconnect(BLEServer* pServer) {
        pServer->getAdvertising()->start();
    }
};

void startBLE(){  
  BLEDevice::init("[AQP] AQUAPONIA");
  BLEDevice::setMTU(517);


  pServer = BLEDevice::createServer();
  // pServer->setCallbacks(new MyServerCallbacks());
  pService = pServer->createService(SERVICE_UUID);
  
  bleSystemInformationCharacteristic = pService->createCharacteristic(CHARACTERISTIC_INFO_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  bleSystemInformationCharacteristic->setCallbacks(&bleSystemInformationCallback);
  bleSystemInformationCharacteristic->setValue("{}");
  
  // bleHistoricoApplyCharacteristic = pService->createCharacteristic(CHARACTERISTIC_GET_HIST_APL_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  // bleHistoricoApplyCharacteristic->setCallbacks(&bleHistoricoApplyGetCallback);
  // bleHistoricoApplyCharacteristic->setValue("{}");
  
  bleHistoricoTempCallback.onReadCallback = getHistTempEndpoint;
  bleHistoricoTempCharacteristic = pService->createCharacteristic(CHARACTERISTIC_GET_HIST_TEMP_UUID,  BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  bleHistoricoTempCharacteristic->setCallbacks(&bleHistoricoTempCallback);
  bleHistoricoTempCharacteristic->setValue("{}");
  
  bleHistoricoPhGetCallback.onReadCallback = getHistPhEndpoint;
  bleHistoricoPhCharacteristic = pService->createCharacteristic(CHARACTERISTIC_GET_HIST_PH_UUID,  BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
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
  bleRoutinesCreateCharacteristic = pServiceRoutinas->createCharacteristic(CHARACTERISTIC_CREATE_ROUTINES_UUID,  BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  bleRoutinesCreateCharacteristic->setCallbacks(&bleRoutinesCreateCallback);
  bleRoutinesCreateCharacteristic->setValue("{}");



  pService->start();
  pServiceRoutinas->start();

  BLEAdvertising  *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->addServiceUUID(SERVICE_ROUTINES_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);

  pAdvertising->start();
}

volatile bool clearEEPROMFlag = false;
void IRAM_ATTR reset() {
  clearEEPROMFlag = true;
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  attachInterrupt(digitalPinToInterrupt(PIN_RESET), reset, RISING);

  aquariumSetupDevice.begin();
  aquarium.begin();

  startBLE();
  startTasks();

  nvs_flash_init();
  nvs_open("storage", NVS_READWRITE, &my_handle);

  nvs_get_u32(my_handle, "remainingTime", &remainingTime);
}

void loop()
{
  if (clearEEPROMFlag) {
    memory.clear();
    ESP.restart();
  }
}
