#include "Clock/Date.h"


#include "Aquarium/Aquarium.h"
#include "Aquarium/Routines.h"
#include "Clock/Clock.h"
#include "rtos/TasksManagement.h"
#include "Base/memory.h"
#include "Aquarium/AquariumServices.h"
#include "Base/config.h"
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
                  *bleRoutinesUpdateCharacteristic,
                  *bleRoutinesGetCharacteristic,
                  *bleRoutinesDeleteCharacteristic,
                  *bleRoutinesCreateCharacteristic;

BluetoothCallback bleConfigurationCallback,
                  blePumpCallback,
                  bleSystemInformationCallback,
                  bleRoutinesUpdateCallback,
                  bleRoutinesDeleteCallback,
                  bleRoutinesGetCallback,
                  bleRoutinesCreateCallback;

bool deviceConnected = false;

Clock clockUTC;
Memory memory;

Aquarium aquarium;
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
SemaphoreHandle_t isExecutingOneWire;
SemaphoreHandle_t isNotify;


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
        Serial.printf("erro: %s\n", e.what());
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
        Serial.printf("erro: %s\n", e.what());
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
    float goalTemperature = (aquarium.getMaxTemperature() - aquarium.getMinTemperature()) / 2 + aquarium.getMinTemperature();
    double erro = goalTemperature - temperature;
    integralError += erro;
    if (temperature == -127.0f)
    {
      aquarium.setStatusHeater(false);
    }
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
      const char* data;
      DynamicJsonDocument doc = aquariumServices.handlerWaterPump();
      deserializeJson(doc, data);
      blePumpCallback.notify(blePumpCharacteristic, data);
    }
    catch (const std::exception& e)
    {
      Serial.printf("[erro] [WATER INFORMATION]: %s\n", e.what());
    }
    vTaskDelay(6000 / portTICK_PERIOD_MS);
  }
}
void TaskPeristaultic()
{
  while (true)
  {
    try
    {
      aquariumServices.controlPeristaultic();
    }
    catch (const std::exception& e)
    {
      Serial.printf("[erro] [PERISTAULTIC INFORMATION]: %s\n", e.what());
    }

    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void startTasks(){
  isExecutingOneWire = xSemaphoreCreateBinary();
  taskOneWire.begin(&TaskOneWireControl, "OneWire", 1000, 1);
  taskTemperatureControl.begin(&TaskAquariumTemperatureControl, "TemperatureAquarium", 1300, 2);
  taskWaterPump.begin(&TaskWaterPump, "WaterPump", 2000, 3);
  taskSendInfo.begin(&TaskSendSystemInformation, "SendInfo", 5000, 4);
}


// Callback para conexão e desconexão
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Cliente conectado");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Cliente desconectado");
    }
};

// // ============================================================================================
// //                                      ENDPOINTS
// // ============================================================================================

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
    Serial.printf("[callback] erro: %s\n", e.what());
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
  for (JsonObject jsonHorario : jsonRoutine["horarios"].as<JsonArray>()) {
    horario h;
    h.start = jsonHorario["start"];
    h.end = jsonHorario["end"];
    r.horarios.push_back(h);
  }
  aquariumServices.setRoutines(r);
  // bleRoutinesGetCharacteristic->notify();
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

  for (JsonObject jsonHorario : jsonRoutine["horarios"].as<JsonArray>()) {
      horario h;
      h.start = jsonHorario["start"];
      h.end = jsonHorario["end"];
      r.horarios.push_back(h);
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


  bleConfigurationCallback.onWriteCallback = updateConfigurationEndpoint;
  bleConfigurationCallback.onReadCallback = getConfigurationEndpoint;

  bleConfigurationCharacteristic = pService->createCharacteristic(CHARACTERISTIC_CONFIGURATION_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  bleConfigurationCharacteristic->setCallbacks(&bleConfigurationCallback);
  bleConfigurationCharacteristic->setValue("{}");

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
  pServer->getAdvertising()->start();
}

void setup()
{
  Serial.begin(115200);
  
  aquarium.begin();
  
  startTasks();
  startBLE();


  memory.write<bool>(ADDRESS_START, true);
  while (1)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void loop()
{
}
