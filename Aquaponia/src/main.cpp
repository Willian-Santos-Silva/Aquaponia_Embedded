#include "Json/Json.h"
#include "Clock/Date.h"

#include "WiFi/LocalNetwork.h"
#include "WiFiServer/LocalServer.h"
#include "WiFiServer/Socket.h"

#include "Aquarium/Aquarium.h"

#include "Clock/Clock.h"
#include "rtos/TasksManagement.h"
#include "Base/memory.h"
#include "Base/config.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-210987654321"

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;

// #include "BluetoothSerial.h"

// BluetoothSerial SerialBT;
bool hasDevice = false;

Memory memory;
LocalNetwork localNetwork;
LocalServer localServer;
Clock clockUTC;
Socket connectionSocket;
Aquarium aquarium;

using namespace std;

// ============================================================================================
//                                      ENDPOINTS
// ============================================================================================

DynamicJsonDocument connectIntoLocalNetwork(AsyncWebServerRequest *request)
{
  DynamicJsonDocument doc(5028);
  JsonObject resp = doc.createNestedObject("data");

  if (!request->hasParam("password") || !request->hasParam("ssid"))
  {
    resp["status_code"] = 500;
    resp["description"] = "Parametro fora de escopo";

    return resp;
  }

  try
  {
    localNetwork.setNetwork(request->getParam("ssid")->value().c_str(), request->getParam("password")->value().c_str());
    localNetwork.openConnection();

    resp["status_code"] = 200;
    resp["description"] = "Conectado com sucesso";
  }

  catch (const std::exception &e)
  {
    resp["status_code"] = 505;
    string err = e.what();
    resp["description"] = err;
    return resp;
  }

  return resp;
}
DynamicJsonDocument updateConfigurationEndpoint(AsyncWebServerRequest *request)
{
  DynamicJsonDocument doc(5028);
  JsonObject resp = doc.createNestedObject("data");

  if (!request->hasParam("min_temperature") || !request->hasParam("max_temperature") || !request->hasParam("ph_min") || !request->hasParam("ph_max") || !request->hasParam("dosagem") || !request->hasParam("ppm"))
  {
    resp["status_code"] = 500;
    resp["description"] = "Parametro fora de escopo";

    return resp;
  }

  try
  {
    int min_temperature = stoi((&request->getParam("min_temperature")->value())->c_str());
    int max_temperature = stoi((&request->getParam("max_temperature")->value())->c_str());
    int ph_min = stoi((&request->getParam("ph_max")->value())->c_str());
    int ph_max = stoi((&request->getParam("ph_min")->value())->c_str());
    int dosagem = stoi((&request->getParam("dosagem")->value())->c_str());
    int ppm = stoi((&request->getParam("ppm")->value())->c_str());

    if (!aquarium.setHeaterAlarm(min_temperature, max_temperature))
    {
      resp["status_code"] = 500;
      resp["description"] = "Falha ao definir intervalo de temperatura, tente novamente";

      return resp;
    }
    if (!aquarium.setPhAlarm(ph_max, ph_max))
    {
      resp["status_code"] = 500;
      resp["description"] = "Falha ao definir intervalo de ph, tente novamente";

      return resp;
    }

    resp["status_code"] = 200;
    resp["description"] = "Temperatura salva com sucesso";
    return resp;
  }

  catch (const std::exception &e)
  {
    resp["status_code"] = 505;
    string err = e.what();
    resp["description"] = err;
    return resp;
  }
}
DynamicJsonDocument getConfigurationEndpoint(AsyncWebServerRequest *request)
{
  
  DynamicJsonDocument doc(5028);
  JsonObject resp = doc.createNestedObject("data");

  try
  {
    
    Serial.print(aquarium.getMinPh());
    Serial.print(" - ");
    Serial.println(aquarium.getMaxPh());

    resp["min_temperature"] = aquarium.getMinTemperature();
    resp["max_temperature"] = aquarium.getMaxTemperature();
    resp["min_ph"] = aquarium.getMinPh();
    resp["max_ph"] = aquarium.getMaxPh();
    resp["dosagem"] = aquarium.getPPM();
    resp["rtc"] = clockUTC.getDateTime().getFullDate();

    resp["status_code"] = 200;
    return resp;
  }
  catch (const std::exception &e)
  {
    resp["status_code"] = 505;
    string err = e.what();
    resp["description"] = err;
    return resp;
  }
}

DynamicJsonDocument getRoutinesEndpoint(AsyncWebServerRequest *request)
{
  DynamicJsonDocument doc(35000);
  JsonObject resp = doc.to<JsonObject>();

  if (!request->hasParam("weekday"))
  {
    resp["status_code"] = 500;
    resp["description"] = "Parametro fora de escopo";

    return resp;
  }
  
  JsonArray dataArray = resp.createNestedArray("data");

  try
  {
    vector<routine> data = aquarium.readRoutine();
    int w = stoi((&request->getParam("weekday")->value())->c_str());
    for (const auto &r : data)
    {
      if(!r.weekday[w]){
        continue;
      }
      JsonObject rotina = dataArray.createNestedObject();
      JsonArray weekdays = rotina.createNestedArray("weekdays");
      for (size_t i = 0; i <  end(r.weekday) - begin(r.weekday); ++i) {
          weekdays.add(r.weekday[i]);
      }

      JsonArray horarios = rotina.createNestedArray("horarios");
      for (const auto &h : r.horarios)
      {
        JsonObject horario = horarios.createNestedObject();
        horario["start"] = h.start;
        horario["end"] = h.end;
      }
    }

    data.clear();
    resp["status_code"] = 200;

    return doc;
  }
  catch (const std::exception &e)
  {
    resp["status_code"] = 505;
    string err = e.what();
    resp["description"] = err;
    return doc;
  }
}
DynamicJsonDocument setRoutinesEndpoint(AsyncWebServerRequest *request)
{
  DynamicJsonDocument doc(5028);
  JsonObject resp = doc.createNestedObject("data");

  resp["status_code"] = 200;
  return resp;
}

// ============================================================================================
//                                      TASKS
// ============================================================================================


TaskWrapper taskBluetoothOnReciveMessage;
TaskWrapper taskBluetoothOnConnect;
TaskWrapper taskBluetoothOnDisconnect;

TaskWrapper taskTemperatureControl;
TaskWrapper taskSendInfo;
TaskWrapper taskWaterPump;
TaskWrapper taskOneWire;
TaskWrapper taskClientConnect;
SemaphoreHandle_t isExecutingOneWire;

void TaskBluetoothOnReciveMessage()
{
  while (true)
  {
    // if (!SerialBT.available()){
    //   vTaskDelay(1000 / portTICK_PERIOD_MS);
    //   continue;
    // }

    // String receivedMessage = "";

    // while (SerialBT.available()) {
    //   char c = (char)SerialBT.read();
      
    //   if(c == '\n')
    //     continue;
      
    //   receivedMessage += c;
    // }

    // Serial.print("Mensagem recebida: ");
    // Serial.println(receivedMessage);
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}
void TaskBluetoothOnConnect()
{
  Serial.println("CONNECT");
  while (true)
  {
    // if (SerialBT.hasClient() && !hasDevice) {
    //   hasDevice = true;
    //   Serial.println("Novo dispositivo conectado!");
    // }
    
    Serial.println("C");

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}
void TaskBluetoothOnDisconnect()
{
  Serial.println("TASK DISCONNECT");
  while (true)
  {
    // if (!SerialBT.hasClient() && hasDevice) {
    //   hasDevice = false;
    //   Serial.println("Dispositivo desconectado!");
    // }
    Serial.println("D");
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void TaskSendSystemInformation()
{
  while (true)
  {
    if(xSemaphoreTake(isExecutingOneWire, portMAX_DELAY))
    {
      DynamicJsonDocument doc(5028);
      JsonObject resp = doc.createNestedObject();
      resp["termopar"] = aquarium.readTemperature();
      resp["min"] = aquarium.getMinTemperature();
      resp["max"] = aquarium.getMaxTemperature();
      resp["rtc"] = clockUTC.getDateTime().getFullDate();
      resp["ph"] = aquarium.getPh();
      resp["ph_v"] = aquarium.getTensao();

      double voltagem = aquarium.getTurbidity() / 4095.0 * 3.3;
      double NTU;
      if (voltagem <= 2.5)
        NTU = 3000;
      else if (voltagem > 4.2)
        NTU = 0;
      else
        NTU = -1120.4 * sqrt(voltagem) + 5742.3 * voltagem - 4353.8;

      resp["tubidity"] = NTU;
      connectionSocket.sendWsData("SystemInformation", resp);
      
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }
  }
}

void TaskOneWireControl()
{
  while (true)
  {
    xSemaphoreGive(isExecutingOneWire);
    aquarium.updateTemperature();
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
    Date now = clockUTC.getDateTime();
    aquarium.handlerWaterPump(now);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
void TaskPeristaultic()
{
  while (true)
  {
    int ph = aquarium.getPh();
    int goal = aquarium.getMinPh() + (aquarium.getMaxPh() - aquarium.getMinPh()) / 2;

    memory.write<long>(ADDRESS_LAST_APPLICATION_ACID_BUFFER_PH, DEFAULT_TIME_DELAY_PH);

    int ml_s = 1 / 1000;

    if (ph <= aquarium.getMinPh())
    {
      int maxBufferSolutionMl = AQUARIUM_VOLUME_L * (DOSAGE_RAISE_SOLUTION_M3_L / 1000);

      aquarium.setPeristaulticStatus(ml_s, aquarium.SOLUTION_RAISER);
    }

    if (ph >= aquarium.getMaxPh())
    {
      int maxAlkalineSolutionMl = AQUARIUM_VOLUME_L * (DOSAGE_LOWER_SOLUTION_M3_L / 1000);

      aquarium.setPeristaulticStatus(ml_s, aquarium.SOLUTION_LOWER);
    }

    vTaskDelay(60000 / portTICK_PERIOD_MS);
  }
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

// Callback para características
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            Serial.println("Mensagem recebida: " + String(value.c_str()));
        }
    }
};

void setup()
{
  Serial.begin(115200);
  // SerialBT.begin("AQP-DEVICE");

// Inicializa o dispositivo BLE
    BLEDevice::init("ESP32_BLE");

    // Cria o servidor BLE
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Cria o serviço BLE
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Cria a característica BLE
    pCharacteristic = pService->createCharacteristic(
                                          CHARACTERISTIC_UUID,
                                          BLECharacteristic::PROPERTY_READ |
                                          BLECharacteristic::PROPERTY_WRITE
                                        );

    // Define o callback para a característica
    pCharacteristic->setCallbacks(new MyCallbacks());

    // Adiciona a característica ao serviço
    pService->start();

    // Inicializa o anúncio
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // Funciona melhor com 0x06
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

  
  aquarium.begin();
  
  localNetwork.openConnection();
  
  localServer.addEndpoint("/configuration/update", &updateConfigurationEndpoint);
  localServer.addEndpoint("/configuration/get", &getConfigurationEndpoint);
  localServer.addEndpoint("/routine/get", &getRoutinesEndpoint);
  localServer.addEndpoint("/routine/update", &setRoutinesEndpoint);
  localServer.addEndpoint("/localNetwork/set", &connectIntoLocalNetwork);
  localServer.init();
  localServer.startSocket(&connectionSocket.socket);
  
  
  isExecutingOneWire = xSemaphoreCreateBinary();
  taskOneWire.begin(&TaskOneWireControl, "OneWire", 1000, 1);
  taskTemperatureControl.begin(&TaskAquariumTemperatureControl, "TemperatureAquarium", 1300, 2);
  taskWaterPump.begin(&TaskWaterPump, "WaterPump", 10000, 3);
  taskSendInfo.begin(&TaskSendSystemInformation, "SendInfo", 10000, 4);

  // taskBluetoothOnReciveMessage.begin(&TaskBluetoothOnReciveMessage, "BluetoothOnReciveMessage", 1000, 5);
  // taskBluetoothOnConnect.begin(&TaskBluetoothOnConnect, "BluetoothOnConnect", 1000, 6);
  // taskBluetoothOnDisconnect.begin(&TaskBluetoothOnDisconnect, "BluetoothOnDisconnect", 1000, 7);

  memory.write<bool>(ADDRESS_START, true);

  while (1)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void loop()
{
}
