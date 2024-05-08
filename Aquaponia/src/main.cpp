#include <functional>
#include <string.h>
#include <regex>

#include "Json/Json.h"
#include "Clock/Date.h"
#include "Connection/LocalWiFi.h"
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
LocalWiFi localWifi;
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

// bool isFloat(const String data)
// {
//   regex rgx("^[+-]?([0-9]+)([.][0-9]+f)?$");
//   smatch match;

//   string s = data.c_str();

//   return regex_match(s, match, rgx);
// }

// ============================================================================================
//                                      ENDPOINTS
// ============================================================================================

Json setTemperatureEndpoint(AsyncWebServerRequest *request)
{

  Json responseData;

  if (!request->hasParam("minTemperature") || !request->hasParam("maxTemperature"))
  {
    responseData.set("status_code", 500);
    responseData.set("description", "Parametro fora de escopo");

    return responseData;
  }

  try
  {
    int minTemperature = tryParseToInt(&request->getParam("minTemperature")->value());
    int maxTemperature = tryParseToInt(&request->getParam("maxTemperature")->value());

    if (!aquarium.setHeaterAlarm(minTemperature, maxTemperature))
    {
      responseData.set("status_code", 500);
      responseData.set("description", "Falha ao definir intervalo de temperatura, tente novamente");

      return responseData;
    }

    responseData.set("status_code", 200);
    responseData.set("description", "Temperatura salva com sucesso");
    responseData.set("min_temperature", aquarium.getMinTemperature());
    responseData.set("max_temperature", aquarium.getMaxTemperature());
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

Json setPhEndpoint(AsyncWebServerRequest *request)
{
  Json responseData;
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
    responseData.set("cooling_status", aquarium.getWaterPumpStatus() ? "on" : "off");

    connectionSocket.sendWsData("SystemInformation", responseData);

    taskSendInfo.finishTask();
    taskSendInfo.awaitTask(taskTemperatureControl);
  }
}


void TaskWaterPump()
{
  while (true)
  {
    aquarium.setWaterPumpStatus(!aquarium.getWaterPumpStatus());
    vTaskDelay(200);
    // taskWaterPump.finishTask();
    // taskWaterPump.awaitTask(taskSendInfo);
  }
}

void setDate(){
  Date d;
  d.day = 5;
  d.month = 5;
  d.year = 2024;
  d.hour = 18;
  d.minute = 10;
  clockUTC.setClock(d);
}


void setup()
{
  Serial.begin(115200);
  setDate();
  localWifi.openConnection();
  connectionSocket.addEndpoint("/setTemperature", &setTemperatureEndpoint);
  connectionSocket.init();
  aquarium.begin();

  taskTemperatureControl.begin(&TaskAquariumTemperatureControl, "TemperatureAquarium", 4096, 1);
  // taskWaterPump.begin(&TaskWaterPump, "WaterPump", 4096, 2);
  taskSendInfo.begin(&TaskSendSystemInformation, "SendInfo", 4096, 3);
}

void loop()
{
}
