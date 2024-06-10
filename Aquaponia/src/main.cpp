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

Memory memory;
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

// ============================================================================================
//                                      ENDPOINTS
// ============================================================================================

Json setTemperatureEndpoint(AsyncWebServerRequest *request)
{

  Json responseData;

  if (!request->hasParam("min_emperature") || !request->hasParam("max_temperature")
   || !request->hasParam("ph_min") || !request->hasParam("ph_max")) || !request->hasParam("dosagem") || !request->hasParam("ppm")  
   )
  {
    responseData.set("status_code", 500);
    responseData.set("description", "Parametro fora de escopo");

    return responseData;
  }

  try
  {
    int minTemperature = tryParseToInt(&request->getParam("min_temperature")->value());
    int maxTemperature = tryParseToInt(&request->getParam("max_temperature")->value());
    int ph_min = tryParseToInt(&request->getParam("max_ph")->value());
    int ph_max = tryParseToInt(&request->getParam("min_ph")->value());
    int dosagem = tryParseToInt(&request->getParam("dosagem")->value());

    if (!aquarium.setHeaterAlarm(minTemperature, maxTemperature))
    {
      responseData.set("status_code", 500);
      responseData.set("description", "Falha ao definir intervalo de temperatura, tente novamente");

      return responseData;
    }
    aquarium.setHeaterAlarm(tempMin, tempMax);
    aquarium.setPhAlarm(phMin, phMax);
    aquarium.setPhAlarm(phMin, phMax);
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

Json setPhEndpoint(AsyncWebServerRequest *request)
{
  Json responseData;
  
  if (!request->hasParam("ph_min") || !request->hasParam("ph_max") || !request->hasParam(""))
  {
    responseData.set("status_code", 500);
    responseData.set("description", "Parametro fora de escopo");

    return responseData;
  }

  return responseData;
}

Json setPhEndpoint(AsyncWebServerRequest *request)
{
  Json responseData;

  if (!request->hasParam("data"))
  {
    responseData.set("status_code", 500);
    responseData.set("description", "Parametro fora de escopo");

    return responseData;
  }


  if (!request->hasParam("data"))
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
    responseData.set("description", e.what());
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

  localWifi.openConnection();
  connectionSocket.addEndpoint("/setTemperature", &setTemperatureEndpoint);
  connectionSocket.init();

  aquarium.begin();
  
  
  taskTemperatureControl.begin(&TaskAquariumTemperatureControl, "TemperatureAquarium", 2730, 1);
  // taskWaterPump.begin(&TaskWaterPump, "WaterPump", 2730, 2);
  taskSendInfo.begin(&TaskSendSystemInformation, "SendInfo", 2730, 3);
}

void loop()
{
}
