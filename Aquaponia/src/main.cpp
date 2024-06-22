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

Json updateConfigurationEndpoint(AsyncWebServerRequest *request)
{

  Json responseData;

  if (!request->hasParam("min_temperature") || !request->hasParam("max_temperature")
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
    int ph_min = tryParseToInt(&request->getParam("ph_max")->value());
    int ph_max = tryParseToInt(&request->getParam("ph_min")->value());
    int dosagem = tryParseToInt(&request->getParam("dosagem")->value());
    int ppm = tryParseToInt(&request->getParam("ppm")->value());

    if (!aquarium.setHeaterAlarm(min_temperature, max_temperature))
    {
      responseData.set("status_code", 500);
      responseData.set("description", "Falha ao definir intervalo de temperatura, tente novamente");

      return responseData;
    }
    if (!aquarium.setPhAlarm(ph_max, ph_max))
    {
      responseData.set("status_code", 500);
      responseData.set("description", "Falha ao definir intervalo de ph, tente novamente");

      return responseData;
    }

    responseData.set("status_code", 200);
    responseData.set("description", "Temperatura salva com sucesso");
    return responseData;
  }

  catch (const std::exception& e)
  {
    responseData.set("status_code", 505);
    string err = e.what();
    responseData.set("description", err);
    return responseData;
  }
}

Json getConfigurationEndpoint(AsyncWebServerRequest *request)
{
  Json responseData;

  try
  {
    responseData.set("min_temperature", aquarium.getMinTemperature());
    responseData.set("max_temperature", aquarium.getMaxTemperature());
    responseData.set("min_ph", aquarium.getMinPh());
    responseData.set("max_ph", aquarium.getMaxPh());
    responseData.set("dosagem", aquarium.getPPM());
    responseData.set("rtc", clockUTC.getDateTime().getFullDate());

    responseData.set("status_code", 200);
    return responseData;
  }
  catch (const std::exception& e)
  {
    responseData.set("status_code", 505);
    string err = e.what();
    responseData.set("description", err);
    return responseData;
  }

}

Json setLocaWifiEndpoint(AsyncWebServerRequest *request)
{
  Json responseData;
  
  if (!request->hasParam("ssid") || !request->hasParam("password"))
  {
    responseData.set("status_code", 500);
    responseData.set("description", "Parametro fora de escopo");

    return responseData;
  }
  responseData.set("status_code", 200);
  responseData.set("description", "Definido com sucesso");

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
    temperature = aquarium.readTemperature();
    Serial.print("Temperatura");
    if (temperature == -127.0f)
    {
      aquarium.setStatusHeater(false);
    }

    float goalTemperature = (aquarium.getMaxTemperature() - aquarium.getMinTemperature()) / 2 + aquarium.getMinTemperature();

    aquarium.setStatusHeater(temperature < aquarium.getMinTemperature() || (aquarium.getHeaterStatus() && temperature < goalTemperature));

    vTaskDelay(20 / portTICK_PERIOD_MS);
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

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void TaskWaterPump()
{
  while (true)
  {
    Serial.print("Pump");
    vTaskDelay(20 / portTICK_PERIOD_MS);
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

  aquarium.begin();

  localWifi.openConnection();
  connectionSocket.addEndpoint("/configuration/update", &updateConfigurationEndpoint);
  connectionSocket.addEndpoint("/configuration/get", &getConfigurationEndpoint);
  connectionSocket.addEndpoint("/LocalConncention/set", &setLocaWifiEndpoint);
  connectionSocket.init();
  
  taskTemperatureControl.begin(&TaskAquariumTemperatureControl, "TemperatureAquarium", 1300, 1);
  taskWaterPump.begin(&TaskWaterPump, "WaterPump", 1300, 2);
  taskSendInfo.begin(&TaskSendSystemInformation, "SendInfo", 2000, 3);

  while (1) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void loop()
{
}
