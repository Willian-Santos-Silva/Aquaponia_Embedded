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

Memory memory;
LocalNetwork localNetwork;
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
    String ssid = request->getParam("ssid")->value();
    String password = request->getParam("password")->value();

    localNetwork.setNetwork(ssid, password);
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
    int min_temperature = tryParseToInt(&request->getParam("min_temperature")->value());
    int max_temperature = tryParseToInt(&request->getParam("max_temperature")->value());
    int ph_min = tryParseToInt(&request->getParam("ph_max")->value());
    int ph_max = tryParseToInt(&request->getParam("ph_min")->value());
    int dosagem = tryParseToInt(&request->getParam("dosagem")->value());
    int ppm = tryParseToInt(&request->getParam("ppm")->value());

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
DynamicJsonDocument setLocaWifiEndpoint(AsyncWebServerRequest *request)
{
  DynamicJsonDocument doc(5028);
  JsonObject responseData = doc.createNestedObject("data");

  if (!request->hasParam("ssid") || !request->hasParam("password"))
  {
    responseData["status_code"] = 500;
    responseData["description"] = "Parametro fora de escopo";

    return responseData;
  }
  responseData["status_code"] = 200;
  responseData["description"] = "Definido com sucesso";

  return responseData;
}
DynamicJsonDocument getRoutinesEndpoint(AsyncWebServerRequest *request)
{
  DynamicJsonDocument doc(5028);
  JsonObject responseData = doc.to<JsonObject>();
  JsonArray dataArray = responseData.createNestedArray("data");
  
  try
  {
    vector<routine> data;
    memory.loadDataFromEEPROM(data);

  for (const auto &r : data)
  {
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

TaskWrapper taskTemperatureControl;
TaskWrapper taskSendInfo;
TaskWrapper taskWaterPump;

void TaskAquariumTemperatureControl()
{
  float temperature;
  float flagTemeperature;
  float Kp = 2.0f, Ki = 5.0f, Kd = 1.0f;
  double integralError;
  double output;
  while (true)
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
void TaskSendSystemInformation()
{
  while (true)
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
    responseData["tubidity_2"] = voltagem;
    responseData["tubidity_3"] = aquarium.getTurbidity();
    connectionSocket.sendWsData("SystemInformation", responseData);

    vTaskDelay(50 / portTICK_PERIOD_MS);
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

void IRAM_ATTR reset()
{
  memory.clear();
}

void setup()
{
  Serial.begin(115200);

  pinMode(PIN_BUTTON_RESET, INPUT);
  attachInterrupt(PIN_BUTTON_RESET, reset, RISING);

  vector<routine> data;

  routine routines;
  routines.weekday[0] = 1;
  routines.weekday[1] = 1;
  routines.weekday[2] = 1;
  routines.weekday[3] = 1;
  routines.weekday[4] = 1;

  horario horario;
  horario.start = 1260;
  horario.end = 1320 - 20;

  routines.horarios.push_back(horario);

  data.push_back(routines);
  data.push_back(routines);
  data.push_back(routines);

  memory.saveDataToEEPROM(data);
  
  DynamicJsonDocument doc(5028);
  JsonArray responseData = doc.createNestedArray("data");
  for (const auto &r : data)
  {
    JsonObject rotina = responseData.createNestedObject();
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
  serializeJson(doc, Serial);

  aquarium.begin();

  localNetwork.openConnection();
  connectionSocket.addEndpoint("/configuration/update", &updateConfigurationEndpoint);
  connectionSocket.addEndpoint("/configuration/get", &getConfigurationEndpoint);
  connectionSocket.addEndpoint("/routine/get", &getRoutinesEndpoint);
  connectionSocket.addEndpoint("/routine/update", &setRoutinesEndpoint);
  connectionSocket.addEndpoint("/LocalConncention/set", &setLocaWifiEndpoint);
  connectionSocket.addEndpoint("/LocalNetwork/set", &connectIntoLocalNetwork);
  connectionSocket.init();

  // taskTemperatureControl.begin(&TaskAquariumTemperatureControl, "TemperatureAquarium", 1300, 1);
  taskWaterPump.begin(&TaskWaterPump, "WaterPump", 10000, 2);
  taskSendInfo.begin(&TaskSendSystemInformation, "SendInfo", 10000, 3);

  while (1)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void loop()
{
}
