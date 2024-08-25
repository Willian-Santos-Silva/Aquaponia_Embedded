#include "Clock/Date.h"

#include "WiFi/LocalNetwork.h"
#include "WiFiServer/LocalServer.h"
#include "WiFiServer/Socket.h"

#include "Aquarium/Aquarium.h"
#include "Clock/Clock.h"
#include "rtos/TasksManagement.h"
#include "Base/memory.h"
#include "Aquarium/AquariumServices.h"
#include "Base/config.h"
#include "Bluetooth/BluetoothCallback.h"
#include "Bluetooth/BluetoothService.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

BLEServer *pServer = NULL;
BLEService *pService;

BLECharacteristic *bleConfigurationCharacteristic;
BLECharacteristic *bleRoutinesCharacteristic;
BLECharacteristic *bleSystemInformationCharacteristic;
BLECharacteristic *bleWiFiCharacteristic;

bool deviceConnected = false;

Clock clockUTC;
Memory memory;

LocalNetwork localNetwork;
LocalServer localServer;
Socket connectionSocket;

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


void TaskSendSystemInformation()
{
  while (true)
  {
    if(xSemaphoreTake(isExecutingOneWire, portMAX_DELAY))
    {
      Serial.println("\n********** SEND INFORMATION **********");
      try
      {
        DynamicJsonDocument doc = aquariumServices.getSystemInformation();

        String resultString;
        serializeJson(doc, resultString);
        
        // bleSystemInformationCharacteristic->setValue(resultString.c_str());
        // bleSystemInformationCharacteristic->notify();
        // connectionSocket.sendWsData("SystemInformation", &doc);
        Serial.printf("data: %s", resultString);
        
        doc.clear();
      }
      catch (const std::exception& e)
      {
          Serial.printf("erro: %s\n", e.what());
      }
      Serial.println("\n**************** END *****************");

      vTaskDelay(50 / portTICK_PERIOD_MS);
    }
  }
}
void TaskOneWireControl()
{
  while (true)
  {
    xSemaphoreGive(isExecutingOneWire);
    Serial.println("\n********** ONE WIRE INFORMATION **********");
    try
    {
      aquarium.updateTemperature();
    }
    catch (const std::exception& e)
    {
        Serial.printf("erro: %s\n", e.what());
    }
    Serial.println("\n**************** END *****************");

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
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
    if(xSemaphoreTake(isExecutingOneWire, portMAX_DELAY))
    {
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
}
void TaskWaterPump()
{
  while (true)
  {
    Serial.println("\n********** WATER INFORMATION **********");
    try
    {
        aquariumServices.handlerWaterPump();
    }
    catch (const std::exception& e)
    {
      Serial.printf("erro: %s\n", e.what());
    }
    Serial.println("\n**************** END *****************");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
void TaskPeristaultic()
{
  while (true)
  {
    Serial.println("\n********** PERISTAULTIC INFORMATION **********");
    try
    {
      aquariumServices.controlPeristaultic();
    }
    catch (const std::exception& e)
    {
        Serial.printf("erro: %s\n", e.what());
    }
    Serial.println("\n**************** END *****************");

    vTaskDelay(60000 / portTICK_PERIOD_MS);
  }
}
void TaskScanWiFiDevices(){

  while (true)
  {
    vector<Hotposts> lHotposts = localNetwork.sanningHotpost();
    DynamicJsonDocument doc(5028);
    JsonArray resp = doc.createNestedArray("data");
    for (const auto &h : lHotposts)
    {
      JsonObject hotpost = resp.createNestedObject();

      hotpost["ssid"] = h.ssid;
      hotpost["rssi"] = h.rssi;
      hotpost["channel"] = h.channel;
      hotpost["encryptionType"] = h.encryptionType;
      
    }
    lHotposts.clear();
    
    String resultString;
    serializeJson(resp, resultString);
    
    // bleWiFiCharacteristic->setValue(resultString.c_str());
    // bleWiFiCharacteristic->notify();
    // connectionSocket.sendWsData("WiFiDevices", &doc);
    Serial.println("============================");
    Serial.println(resultString);
    Serial.println("============================");
    
    doc.clear();
    resp.clear();
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void startTasks(){
  isExecutingOneWire = xSemaphoreCreateBinary();
  taskOneWire.begin(&TaskOneWireControl, "OneWire", 1000, 1);
  taskTemperatureControl.begin(&TaskAquariumTemperatureControl, "TemperatureAquarium", 1300, 2);
  taskWaterPump.begin(&TaskWaterPump, "WaterPump", 10000, 3);
  taskSendInfo.begin(&TaskSendSystemInformation, "SendInfo", 10000, 4);
  taskScanWiFiDevices.begin(&TaskScanWiFiDevices, "ScanningWiFi", 10000, 5);
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
  if (!doc->containsKey("min_temperature") || !doc->containsKey("max_temperature") || 
      !doc->containsKey("ph_min") || !doc->containsKey("ph_max") || 
      !doc->containsKey("dosagem") || !doc->containsKey("ppm"))
  {
    throw std::runtime_error("Parametro fora de escopo");
  }

  aquariumServices.updateConfiguration((*doc)["min_temperature"].as<int>(),
                                       (*doc)["max_temperature"].as<int>(),
                                       (*doc)["ph_max"].as<int>(),
                                       (*doc)["ph_min"].as<int>(),
                                       (*doc)["dosagem"].as<int>(),
                                       (*doc)["ppm"].as<int>());
  return (*doc);
}
DynamicJsonDocument getConfigurationEndpoint(DynamicJsonDocument *doc)
{
  DynamicJsonDocument response(5028);

  response["data"] = aquariumServices.getConfiguration();
  response["status_code"] = 200;

  return response;
}
DynamicJsonDocument getRoutinesEndpoint(DynamicJsonDocument *doc)
{
  if (!doc->containsKey("weekday"))
  {
    throw std::runtime_error("Parametro fora de escopo");
  }

  int weekday = (*doc)["weekday"].as<int>();
  return  aquariumServices.getRoutines(weekday);
}
DynamicJsonDocument setRoutinesEndpoint(DynamicJsonDocument *doc)
{
  DynamicJsonDocument resp(5028);
  resp["status_code"] = 200;

  return resp;
}


// ============================================================================================
//                                      START SERVICES
// ============================================================================================

DynamicJsonDocument onTryConnectWiFi(DynamicJsonDocument *doc){
  
  const char* ssid = (*doc)["ssid"].as<const char*>();
  const char* password = (*doc)["password"].as<const char*>();

  Serial.printf("\r\n\nSSID: %s\r\nSenha: %s\r\n\n", ssid, password);

  localNetwork.StartSTA(ssid, password);

  localServer.init();
  localServer.startSocket(&connectionSocket.socket);

  DynamicJsonDocument resp (200);
  resp["ip"] = localNetwork.GetIp().c_str();
  return resp;
}

  BluetoothCallback bleConfigurationCallback;
  BluetoothCallback bleWiFiCallback;
  BluetoothCallback bleSystemInformationCallback;
void startBLE(){  
  BLEDevice::init("[AQP] AQUAPONIA");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  pService = pServer->createService(SERVICE_UUID);


  bleWiFiCallback.onWriteCallback = onTryConnectWiFi;

  bleWiFiCharacteristic = pService->createCharacteristic(CHARACTERISTIC_WIFI_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  bleWiFiCharacteristic->setCallbacks(&bleWiFiCallback);
  bleWiFiCharacteristic->setValue("");

  
  bleSystemInformationCharacteristic = pService->createCharacteristic(CHARACTERISTIC_INFO_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  bleSystemInformationCharacteristic->setCallbacks(&bleSystemInformationCallback);
  bleSystemInformationCharacteristic->setValue("");


  bleConfigurationCallback.onWriteCallback = updateConfigurationEndpoint;
  bleConfigurationCallback.onReadCallback = getConfigurationEndpoint;

  bleConfigurationCharacteristic = pService->createCharacteristic(CHARACTERISTIC_CONFIGURATION_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  bleConfigurationCharacteristic->setCallbacks(&bleConfigurationCallback);
  bleConfigurationCharacteristic->setValue("");

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();
}

void setup()
{
  Serial.begin(115200);
  
  aquarium.begin();
  
  startTasks();
  startBLE();
  // //startServer();                   true);
  localServer.addEndpoint("/configuration/update", &updateConfigurationEndpoint);
  localServer.addEndpoint("/configuration/get", &getConfigurationEndpoint);
  localServer.addEndpoint("/routine/get", &getRoutinesEndpoint);
  localServer.addEndpoint("/routine/update", &setRoutinesEndpoint);


  memory.write<bool>(ADDRESS_START, true);
  while (1)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void loop()
{
}
