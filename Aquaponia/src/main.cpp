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

TaskWrapper taskReadTemperature;
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
    vTaskDelay(pdMS_TO_TICKS(3600000.0));
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
      vTaskDelay(pdMS_TO_TICKS(1000.0));
      continue;
    }
    
    double goalTemperature = (aquarium.getMaxTemperature() - aquarium.getMinTemperature()) / 2 + aquarium.getMinTemperature();
    double erro = goalTemperature - temperature;
    integralError += erro;

    flagTemeperature = temperature;
    output = Kp * erro + Ki * integralError - Kd * (temperature - flagTemeperature);

    aquarium.setStatusHeater(output < 0);

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
  taskPeristaultic.begin(&TaskPeristaultic, "Peristautic", 5000, 1); 
  taskTemperatureControl.begin(&TaskAquariumTemperatureControl, "TemperatureAquarium", 1300, 2);
  taskSaveLeitura.begin(&TaskSaveLeitura, "SaveTemperatura", 5000, 3);
  taskWaterPump.begin(&TaskWaterPump, "WaterPump", 3000, 4);
  taskSendInfo.begin(&TaskSendSystemInformation, "SendInfo", 3000, 5);
}



// // ============================================================================================
// //                                      ENDPOINTS
// // ============================================================================================


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

  taskPeristaultic.pause();
  taskTemperatureControl.pause();
  taskSaveLeitura.pause();
  taskWaterPump.pause();

  if (!(*doc)["rtc"].is<long>())
  {
    taskPeristaultic.resume();
    taskTemperatureControl.resume();
    taskSaveLeitura.resume();
    taskWaterPump.resume();
    throw std::runtime_error("Parametro fora de escopo");
  }

  long timestamp =(*doc)["rtc"].as<long>();

  time_t timestamp_t = timestamp;
  tm * time = gmtime(&timestamp_t);
  clockUTC.setRTC(time);

  taskPeristaultic.resume();
  taskTemperatureControl.resume();
  taskSaveLeitura.resume();
  taskWaterPump.resume();

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
  // taskPeristaultic.pause();
  // taskTemperatureControl.pause();
  // taskSaveLeitura.pause();
  // taskWaterPump.pause();

  aquariumServices.updateConfiguration((*doc)["min_temperature"].as<int>(),
                                        (*doc)["max_temperature"].as<int>(),
                                        (*doc)["min_ph"].as<int>(),
                                        (*doc)["max_ph"].as<int>(),
                                        (*doc)["dosagem_solucao_acida"].as<int>(),
                                        (*doc)["dosagem_solucao_base"].as<int>(),
                                        (*doc)["tempo_reaplicacao"].as<long>());

  // taskPeristaultic.resume();
  // taskTemperatureControl.resume();
  // taskSaveLeitura.resume();
  // taskWaterPump.resume();

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

// class MyServerCallbacks: public NimBLEServerCallbacks {
//     void onConnect(BLEServer* pServer) {
//     }
//     void onDisconnect(BLEServer* pServer) {
//         pServer->getAdvertising()->start();
//     }
// };

void startBLE(){  
  NimBLEDevice::init("[AQP] AQUAPONIA");
  NimBLEDevice::setMTU(517);


  pServer = NimBLEDevice::createServer();
  // pServer->setCallbacks(new MyServerCallbacks());
  pService = pServer->createService(SERVICE_UUID);
  
  bleSystemInformationCharacteristic = pService->createCharacteristic(CHARACTERISTIC_INFO_UUID, NIMBLE_PROPERTY::NOTIFY);
  bleSystemInformationCharacteristic->setCallbacks(&bleSystemInformationCallback);
  bleSystemInformationCharacteristic->setValue("{}");
  
  // bleHistoricoApplyCharacteristic = pService->createCharacteristic(CHARACTERISTIC_GET_HIST_APL_UUID, NIMBLE_PROPERTY::NOTIFY);
  // bleHistoricoApplyCharacteristic->setCallbacks(&bleHistoricoApplyGetCallback);
  // bleHistoricoApplyCharacteristic->setValue("{}");
  
  bleHistoricoTempCallback.onReadCallback = getHistTempEndpoint;
  bleHistoricoTempCharacteristic = pService->createCharacteristic(CHARACTERISTIC_GET_HIST_TEMP_UUID,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  bleHistoricoTempCharacteristic->setCallbacks(&bleHistoricoTempCallback);
  bleHistoricoTempCharacteristic->setValue("{}");
  
  bleHistoricoPhGetCallback.onReadCallback = getHistPhEndpoint;
  bleHistoricoPhCharacteristic = pService->createCharacteristic(CHARACTERISTIC_GET_HIST_PH_UUID,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
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


static void IRAM_ATTR reset() {
  // log_e("Reset");
  // vTaskSuspendAll();
  memory.clear();
  ESP.restart();
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
}

void loop()
{
  vTaskSuspend(NULL);
}
