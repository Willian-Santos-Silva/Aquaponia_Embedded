
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

#include "BluetoothSerial.h"

BluetoothSerial SerialBT;
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
  JsonObject responseData = doc.createNestedObject("data");

  if (!request->hasParam("password") || !request->hasParam("ssid"))
  {
    responseData["status_code"] = 500;
    responseData["description"] = "Parametro fora de escopo";

    return responseData;
  }

  try
  {
    localNetwork.setNetwork(request->getParam("ssid")->value().c_str(), request->getParam("password")->value().c_str());
    localNetwork.openConnection();

    responseData["status_code"] = 200;
    responseData["description"] = "Conectado com sucesso";
  }

  catch (const std::exception &e)
  {
    responseData["status_code"] = 505;
    string err = e.what();
    responseData["description"] = err;
    return responseData;
  }

  return responseData;
}
DynamicJsonDocument updateConfigurationEndpoint(AsyncWebServerRequest *request)
{
  DynamicJsonDocument doc(5028);
  JsonObject responseData = doc.createNestedObject("data");

  if (!request->hasParam("min_temperature") || !request->hasParam("max_temperature") || !request->hasParam("ph_min") || !request->hasParam("ph_max") || !request->hasParam("dosagem") || !request->hasParam("ppm"))
  {
    responseData["status_code"] = 500;
    responseData["description"] = "Parametro fora de escopo";

    return responseData;
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
      responseData["status_code"] = 500;
      responseData["description"] = "Falha ao definir intervalo de temperatura, tente novamente";

      return responseData;
    }
    if (!aquarium.setPhAlarm(ph_max, ph_max))
    {
      responseData["status_code"] = 500;
      responseData["description"] = "Falha ao definir intervalo de ph, tente novamente";

      return responseData;
    }

    responseData["status_code"] = 200;
    responseData["description"] = "Temperatura salva com sucesso";
    return responseData;
  }

  catch (const std::exception &e)
  {
    responseData["status_code"] = 505;
    string err = e.what();
    responseData["description"] = err;
    return responseData;
  }
}
DynamicJsonDocument getConfigurationEndpoint(AsyncWebServerRequest *request)
{
  
  DynamicJsonDocument doc(5028);
  JsonObject responseData = doc.createNestedObject("data");

  try
  {
    
    Serial.print(aquarium.getMinPh());
    Serial.print(" - ");
    Serial.println(aquarium.getMaxPh());

    responseData["min_temperature"] = aquarium.getMinTemperature();
    responseData["max_temperature"] = aquarium.getMaxTemperature();
    responseData["min_ph"] = aquarium.getMinPh();
    responseData["max_ph"] = aquarium.getMaxPh();
    responseData["dosagem"] = aquarium.getPPM();
    responseData["rtc"] = clockUTC.getDateTime().getFullDate();

    responseData["status_code"] = 200;
    return responseData;
  }
  catch (const std::exception &e)
  {
    responseData["status_code"] = 505;
    string err = e.what();
    responseData["description"] = err;
    return responseData;
  }
}

DynamicJsonDocument getRoutinesEndpoint(AsyncWebServerRequest *request)
{
  DynamicJsonDocument doc(35000);
  JsonObject responseData = doc.to<JsonObject>();

  if (!request->hasParam("weekday"))
  {
    responseData["status_code"] = 500;
    responseData["description"] = "Parametro fora de escopo";

    return responseData;
  }
  
  JsonArray dataArray = responseData.createNestedArray("data");

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
    responseData["status_code"] = 200;

    return doc;
  }
  catch (const std::exception &e)
  {
    responseData["status_code"] = 505;
    string err = e.what();
    responseData["description"] = err;
    return doc;
  }
}
DynamicJsonDocument setRoutinesEndpoint(AsyncWebServerRequest *request)
{
  DynamicJsonDocument doc(5028);
  JsonObject responseData = doc.createNestedObject("data");

  responseData["status_code"] = 200;
  return responseData;
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
    if (!SerialBT.available()){
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    String receivedMessage = "";

    while (SerialBT.available()) {
      char c = (char)SerialBT.read();
      
      if(c == '\n')
        continue;
      
      receivedMessage += c;
    }

    Serial.print("Mensagem recebida: ");
    Serial.println(receivedMessage);
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void TaskBluetoothOnConnect()
{
  while (true)
  {
    if (SerialBT.hasClient() && !hasDevice) {
      hasDevice = true;
      Serial.println("Novo dispositivo conectado!");
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void TaskBluetoothOnDisconnect()
{
  while (true)
  {
    if (!SerialBT.hasClient() && hasDevice) {
      hasDevice = false;
      Serial.println("Dispositivo desconectado!");
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
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
void TaskSendSystemInformation()
{
  while (true)
  {
    if(xSemaphoreTake(isExecutingOneWire, portMAX_DELAY))
    {
      DynamicJsonDocument doc(5028);
      JsonObject responseData = doc.createNestedObject();
      responseData["termopar"] = aquarium.readTemperature();
      responseData["min"] = aquarium.getMinTemperature();
      responseData["max"] = aquarium.getMaxTemperature();
      responseData["rtc"] = clockUTC.getDateTime().getFullDate();
      responseData["ph"] = aquarium.getPh();
      responseData["ph_v"] = aquarium.getTensao();

      double voltagem = aquarium.getTurbidity() / 4095.0 * 3.3;
      double NTU;
      if (voltagem <= 2.5)
        NTU = 3000;
      else if (voltagem > 4.2)
        NTU = 0;
      else
        NTU = -1120.4 * sqrt(voltagem) + 5742.3 * voltagem - 4353.8;

      responseData["tubidity"] = NTU;
      connectionSocket.sendWsData("SystemInformation", responseData);
      
      vTaskDelay(50 / portTICK_PERIOD_MS);
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

void setup()
{
  Serial.begin(115200);
  SerialBT.begin("AQP-DEVICE");
  
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

  // taskBluetoothOnReciveMessage.begin(&TaskBluetoothOnReciveMessage, "BluetoothOnReciveMessage", 100, 3);
  // taskBluetoothOnConnect.begin(&TaskBluetoothOnConnect, "BluetoothOnConnect", 100, 3);
  // taskBluetoothOnDisconnect.begin(&TaskBluetoothOnDisconnect, "BluetoothOnDisconnect", 100, 3);

  memory.write<bool>(ADDRESS_START, true);

  while (1)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void loop()
{
}
