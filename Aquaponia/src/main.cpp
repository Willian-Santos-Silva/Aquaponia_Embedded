#include <functional>
#include <string.h>
#include <regex>

#include "Json/Json.h"
#include "Clock/Date.h"
#include "Connection/LocalNetwork.h"
#include "Socket/Socket.h"
#include "Aquarium/Aquarium.h"

#include "Clock/Clock.h"
#include "rtos/TasksManagement.h"
#include "memory.h"
#include "config.h"
#define PRINT_VARIABLE_NAME(variable) printVariable(#variable);

string printVariable(const char *name)
{
  return name;
}

Memory memory;
LocalNetwork localNetwork;
LocalWifi localWifi;
Clock clockUTC;
Socket connectionSocket;
Aquarium aquarium;

using namespace std;

// ============================================================================================
//                                      CONVERT
// ============================================================================================

int tryParseToInt(const String *data)
{
  regex rgx("^[+-]?[0-9]+$");
  smatch match;

  string strToInt = data->c_str();

  if (!regex_match(strToInt, match, rgx))
    throw invalid_argument("Error: Impossivel converter a variavel para Int");

  if (sizeof(strToInt) > sizeof(int))
    throw out_of_range("Error: Parametro fora do intervalo");

  return stoi(strToInt);
}

// ============================================================================================
//                                      ENDPOINTS
// ============================================================================================

Json connectIntoLocalNetwork(AsyncWebServerRequest *request)
{

  Json responseData;

  if (!request->hasParam("password") || !request->hasParam("ssid"))
  {
    responseData.set("status_code", 500);
    responseData.set("description", "Parametro fora de escopo");

    return responseData;
  }

  try
  {
    int ssid = tryParseToInt(&request->getParam("ssid")->value());
    int password = tryParseToInt(&request->getParam("password")->value());
    if (!aquarium.setHeaterAlarm(ssid, password))
    {
      responseData.set("status_code", 500);
      responseData.set("description", "Falha se conectar a rede, tente novamente");

      return responseData;
    }
    localNetwork.setNetwork(ssid, password);
    localNetwork.openConnection();

    responseData.set("status_code", 200);
    responseData.set("description", "Conectado com sucesso");
  }

  catch (const std::exception& e)
  {
    responseData.set("status_code", 505);
    string err = e.what();
    responseData.set("description", err);
    return responseData;
  }

  return responseData;
}

Json updateConfigurationEndpoint(AsyncWebServerRequest *request)
{

  Json responseData;

  if (!request->hasParam("min_emperature") || !request->hasParam("max_temperature")
   || !request->hasParam("ph_min") || !request->hasParam("ph_max")
   || !request->hasParam("dosagem") || !request->hasParam("ppm")  
   )
  {
    responseData.set("status_code", 500);
    responseData.set("description", "Parametro fora de escopo");

    return responseData;
  }

  try
  {
    int min_temperature = tryParseToInt(&request->getParam("min_temperature")->value());
    int max_temperature = tryParseToInt(&request->getParam("max_temperature")->value());
    int ph_min = tryParseToInt(&request->getParam("max_ph")->value());
    int ph_max = tryParseToInt(&request->getParam("min_ph")->value());
    int dosagem = tryParseToInt(&request->getParam("dosagem")->value());

    if (!aquarium.setHeaterAlarm(min_temperature, max_temperature))
    {
      responseData.set("status_code", 500);
      responseData.set("description", "Falha ao definir intervalo de temperatura, tente novamente");

      return responseData;
    }
    aquarium.setHeaterAlarm(min_temperature, max_temperature);
    aquarium.setPhAlarm(ph_max, ph_max);
    responseData.set("status_code", 200);
    responseData.set("description", "Temperatura salva com sucesso");
  }

  catch (const std::exception& e)
  {
    responseData.set("status_code", 505);
    string err = e.what();
    responseData.set("description", err);
    return responseData;
  }

  return responseData;
}

Json getConfigurationEndpoint(AsyncWebServerRequest *request)
{
  Json responseData;

  try
  {

    Json responseData;
    responseData.set("min_temperature", aquarium.getMinTemperature());
    responseData.set("max_temperature", aquarium.getMaxTemperature());
    responseData.set("min_ph", aquarium.getMinPh());
    responseData.set("max_ph", aquarium.getMaxPh());
    responseData.set("dosagem", aquarium.getPPM());
    responseData.set("rtc", clockUTC.getDateTime().getFullDate());

    responseData.set("status_code", 200);
  }

  catch (const std::exception& e)
  {
    responseData.set("status_code", 505);
    string err = e.what();
    responseData.set("description", err);
    return responseData;
  }

  return responseData;
}
// ============================================================================================
//                                      TASKS
// ============================================================================================

TaskWrapper taskTemperatureControl;
TaskWrapper taskSendInfo;
TaskWrapper taskWaterPump;

void TaskAquariumTemperatureControl()
{
  float temperature;

  while (true)
  {
    taskSendInfo.awaitTask(taskSendInfo);

    temperature = aquarium.readTemperature();

    if (temperature == -127.0f)
    {
      aquarium.setStatusHeater(false);

      taskTemperatureControl.finishTask();
    }

    float goalTemperature = (aquarium.getMaxTemperature() - aquarium.getMinTemperature()) / 2 + aquarium.getMinTemperature();

    aquarium.setStatusHeater(temperature < aquarium.getMinTemperature() || (aquarium.getHeaterStatus() && temperature < goalTemperature));

    taskTemperatureControl.finishTask();
  }
}

void TaskSendSystemInformation()
{
  float temperature;

  while (true)
  {
    temperature = aquarium.readTemperature();

    Json responseData;
    responseData.set("termopar", temperature);
    responseData.set("min", aquarium.getMinTemperature());
    responseData.set("max", aquarium.getMaxTemperature());
    responseData.set("rtc", clockUTC.getDateTime().getFullDate());
    responseData.set("heater_status", aquarium.getHeaterStatus() ? "on" : "off");
    responseData.set("ph", aquarium.getPh());
    responseData.set("tubidity", aquarium.getTurbidity());
    connectionSocket.sendWsData("SystemInformation", responseData);

    // responseData.set("ph_me", aquarium.getPhByME());
    // responseData.set("volt_ph", aquarium.getPhDDP());
    // responseData.set("millivolt_ph", analogReadMilliVolts(PIN_PH));
    // responseData.set("adc_ph", analogRead(PIN_PH));

    taskSendInfo.finishTask();
    taskSendInfo.awaitTask(taskTemperatureControl);
  }
}


void Task()
{
  while (true)
  {
    // aquarium.setWaterPumpStatus(!aquarium.getWaterPumpStatus());
    Serial.print("Liga");
    vTaskDelay(2000/portTICK_PERIOD_MS);
    // taskWaterPump.finishTask();
    // taskWaterPump.awaitTask(taskSendInfo);
  }
}

void TaskWaterPump()
{
  while (true)
  {
    // aquarium.setWaterPumpStatus(!aquarium.getWaterPumpStatus());
    Serial.print("Liga");
    vTaskDelay(2000/portTICK_PERIOD_MS);
    // taskWaterPump.finishTask();
    // taskWaterPump.awaitTask(taskSendInfo);
  }
}
void IRAM_ATTR reset() {
  Serial.print("Reset");
  memory.clear();
}

void setup()
{
  Serial.begin(115200);

  pinMode(PIN_BUTTON_RESET, INPUT);
  attachInterrupt(PIN_BUTTON_RESET, reset, RISING);

  localNetwork.openConnection();
  connectionSocket.addEndpoint("/configuration/update", &updateConfigurationEndpoint);
  connectionSocket.addEndpoint("/configuration/get", &getConfigurationEndpoint);
  connectionSocket.addEndpoint("/LocalNetwork/set", &connectIntoLocalNetwork);
  connectionSocket.init();

  aquarium.begin();
  
  
  taskTemperatureControl.begin(&TaskAquariumTemperatureControl, "TemperatureAquarium", 2730, 1);
  // taskWaterPump.begin(&TaskWaterPump, "WaterPump", 2730, 2);
  taskSendInfo.begin(&TaskSendSystemInformation, "SendInfo", 2730, 3);
}

void loop()
{
}
