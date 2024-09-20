#ifndef AQUARIUM_SERVICES_H
#define AQUARIUM_SERVICES_H

#include <vector>

#include <ArduinoJson.h>
#include "Clock/Date.h"
#include "Aquarium.h"
#include "SetupDevice.h"
#include "Aplicacoes.h"
#include "Routines.h"

#include "Clock/Clock.h"

using namespace std;


JsonDocument  routineVectorToJSON(vector<routine> & data) {
  JsonDocument doc;
  JsonArray dataArray = doc.to<JsonArray>();
  
  for (const auto& r : data) {
      JsonObject rotina = dataArray.add<JsonObject>();
      rotina["id"] = r.id;

      for (size_t i = 0; i < sizeof(r.weekday) / sizeof(r.weekday[0]); ++i) {
          rotina["weekdays"][i] = r.weekday[i];
      }
      
      for (int i = 0; i < 720; i++) {
          horario h = r.horarios[i];

          if(h.start == 1441 || h.end == 1441)
              continue;
          
          rotina["horarios"][i]["start"] = h.start;
          rotina["horarios"][i]["end"] = h.end;
      }
      dataArray.add(rotina);
  }
  
  return doc;
}

class AquariumServices
{
private:
    Aquarium* _aquarium;
    Clock clockUTC;

    aplicacoes applySolution(const vector<aplicacoes>& aplicacaoList, Aquarium::solution so);
    aplicacoes applyRiseSolution(double deltaPh, double acumuladoAplicado);
    aplicacoes applyLowerSolution(double deltaPh, double acumuladoAplicado);
    void handlerWaterPump(Date now);
    
public:
    AquariumServices(Aquarium *aquarium);
    ~AquariumServices();

    void updateConfiguration(int min_temperature, int max_temperature, int ph_min, int ph_max, int dosagem, int tempo_reaplicacao);
    JsonDocument  getSystemInformation();
    JsonDocument  getHistPh();
    JsonDocument  getHistTemp();
    JsonDocument  getRoutines();
    JsonDocument  getConfiguration();
    void setRoutines(routine& r);
    void addRoutines(routine& r);
    void removeRoutine(char id[36]);
    void controlPeristaultic();
    JsonDocument  handlerWaterPump();
};

AquariumServices::AquariumServices(Aquarium *aquarium) : _aquarium(aquarium)
{
}

AquariumServices::~AquariumServices()
{
}

JsonDocument  AquariumServices::getSystemInformation()
{
    JsonDocument  doc;
    doc["temperatura"] = _aquarium->readTemperature();
    doc["rtc"] = clockUTC.getDateTimeString();
    doc["ph"] = _aquarium->getPh();
    doc["ntu"] = _aquarium->getTurbidity();
    doc["status_heater"] = _aquarium->getHeaterStatus();
    doc["status_water_pump"] = _aquarium->getWaterPumpStatus();
    
    return doc;
}


const char* printData(time_t *data) {

    struct tm *timeinfo = gmtime(data);

    static char dateTimeStr[30];
    snprintf(dateTimeStr, sizeof(dateTimeStr), "%02d/%02d/%04d %02d:%02d:%02d",
        timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900,
        timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    // Serial.printf("DATA: %s\r\n\r\n", dateTimeStr);
    return dateTimeStr;
}

time_t tmToTimestamp(tm * time) {
    return mktime(time); 
}


JsonDocument  AquariumServices::getHistPh(){
    JsonDocument  doc;
    JsonArray dataArray = doc.to<JsonArray>();

    SetupDevice _setupDevice(_aquarium);
    vector<historicoPh> list = _setupDevice.read<historicoPh>("/histTemp.bin");
    for (const auto& data : list) {
        JsonObject dataObj;
        dataObj["ph"] = data.ph;
        dataObj["timestamp"] = data.time;

        dataArray.add(dataObj);
    }

    dataArray.clear();
  
    return doc;
}

JsonDocument  AquariumServices::getHistTemp(){
    JsonDocument  doc;
    JsonArray dataArray = doc.to<JsonArray>();

    SetupDevice _setupDevice(_aquarium);
    vector<historicoTemperatura> list = _setupDevice.read<historicoTemperatura>("/histPh.bin");
    for (const auto& data : list) {
        JsonObject dataObj;
        dataObj["temperatura"] = data.temperatura;
        dataObj["timestamp"] = data.time;
        dataArray.add(dataObj);
    }

    dataArray.clear();
  
    return doc;
}


void AquariumServices::controlPeristaultic() {
    SetupDevice _setupDevice(_aquarium);
    vector<aplicacoes> aplicacaoList = _setupDevice.read<aplicacoes>("/pump.bin");

    aplicacoes applyRaiser = applySolution(aplicacaoList, _aquarium->SOLUTION_RAISER);
    

    if(applyRaiser.ml > 0.0) {
        Serial.printf("Dosagem Raiser: %lf\r\n\r\n", applyRaiser.ml);

        aplicacaoList.push_back(applyRaiser);

        if(aplicacaoList.size() > 10)
            aplicacaoList.erase(aplicacaoList.begin());

        _setupDevice.write<aplicacoes>(aplicacaoList, "/pump.bin");
    }


    aplicacoes applyLower = applySolution(aplicacaoList, _aquarium->SOLUTION_LOWER);
    if(applyLower.ml > 0.0){
        Serial.printf("Dosagem Lower: %lf\r\n\r\n", applyLower.ml);

        aplicacaoList.push_back(applyLower);
        
        if(aplicacaoList.size() > 10)
            aplicacaoList.erase(aplicacaoList.begin());
            
    SetupDevice _setupDevice(_aquarium);
        _setupDevice.write<aplicacoes>(aplicacaoList, "/pump.bin");
    }
}

aplicacoes AquariumServices::applySolution(const vector<aplicacoes>& aplicacaoList, Aquarium::solution solution) {
    aplicacoes ultimaAplicacao;
    aplicacoes primeiraAplicacao;

    struct tm *timeUltimaAplicacao;
    time_t dataProximaAplicacao;
    bool hasLast = false;


    double deltaPh;
    double acumuladoAplicado = 0;

    tm now = clockUTC.getDateTime();
    tm *nowp = &now;
    time_t timestamp = mktime(nowp); 

    for (uint32_t i = aplicacaoList.size(); i > 0; i--) {
        aplicacoes aplicacao = aplicacaoList[i - 1];

        if(aplicacao.type != solution)
            continue;
        primeiraAplicacao = aplicacao;
            
        if(!hasLast){
            ultimaAplicacao = aplicacao;
            hasLast = true;
        }

        if((long)ultimaAplicacao.dataAplicacao - (long)aplicacao.dataAplicacao >= DEFAULT_TIME_DELAY_PH
         || ((long)timestamp - (long)ultimaAplicacao.dataAplicacao) >=  DEFAULT_TIME_DELAY_PH)
            break;

        acumuladoAplicado += aplicacao.ml;
    }

    if(hasLast){
        Serial.printf("DATA ULTIMA: %s\r\n", printData(&ultimaAplicacao.dataAplicacao));
    }

    aplicacoes aplicacao;

    if(solution == Aquarium::SOLUTION_LOWER){
        aplicacao = applyLowerSolution(0, acumuladoAplicado);
        aplicacao.type = solution;
        aplicacao.dataAplicacao = timestamp;
        return aplicacao;
    }

    if(solution == Aquarium::SOLUTION_RAISER){
        aplicacao = applyRiseSolution(0, acumuladoAplicado);
        aplicacao.type = solution;
        aplicacao.dataAplicacao = timestamp;
        return aplicacao;
    }

    return aplicacao;
}

aplicacoes AquariumServices::applyRiseSolution(double deltaPh, double acumuladoAplicado) {
    aplicacoes aplicacao;
    aplicacao.ml = 0.0;

    double ml_s = 1.0;

    float initialPh = _aquarium->getPh();
    int goal = _aquarium->getMinPh() + (_aquarium->getMaxPh() - _aquarium->getMinPh()) / 2;
    
    double maxBufferSolutionMl = AQUARIUM_VOLUME_L /   DOSAGE_RAISE_SOLUTION_ML_L;

    while ((acumuladoAplicado + aplicacao.ml + ml_s) <= maxBufferSolutionMl) {
        float ph = _aquarium->getPh();

        if (ph >= _aquarium->getMinPh())
        {
            break;
        }

        _aquarium->setPeristaulticStatus(ml_s, _aquarium->SOLUTION_RAISER);
        
        aplicacao.ml += ml_s;
        deltaPh = ph - initialPh;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    Serial.printf("APLICADO: %lf\r\n", aplicacao.ml);

    return aplicacao;
}

aplicacoes AquariumServices::applyLowerSolution(double deltaPh, double acumuladoAplicado) {
    aplicacoes aplicacao;
    aplicacao.ml = 0.0;
    double ml_s = 1.0;

    float initialPh = _aquarium->getPh();
    int goal = _aquarium->getMinPh() + (_aquarium->getMaxPh() - _aquarium->getMinPh()) / 2;
    
    double maxAlkalineSolutionMl = AQUARIUM_VOLUME_L / DOSAGE_LOWER_SOLUTION_ML_L;

    while ((acumuladoAplicado + aplicacao.ml + ml_s) <= maxAlkalineSolutionMl) {
        float ph = _aquarium->getPh();

        if (ph <= _aquarium->getMaxPh())
        {
            break;
        }

        _aquarium->setPeristaulticStatus(ml_s, _aquarium->SOLUTION_LOWER);
        
        aplicacao.ml += ml_s;
        deltaPh = ph - initialPh;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    aplicacao.deltaPh = deltaPh;

    return aplicacao;
}


JsonDocument  AquariumServices::getConfiguration(){
    JsonDocument doc;

    doc["min_temperature"] = _aquarium->getMinTemperature();
    doc["max_temperature"] = _aquarium->getMaxTemperature();
    doc["min_ph"] = _aquarium->getMinPh();
    doc["max_ph"] = _aquarium->getMaxPh();
    doc["ppm"] = _aquarium->getPPM();
    // doc["rtc"] = clockUTC.getDateTime().getFullDate();

    return doc;
}

void printRotinas(vector<routine> &data){    
    for (const auto& r : data) {
        Serial.printf("%s\r\n",r.id);
        for (size_t i = 0; i < sizeof(r.weekday) / sizeof(r.weekday[0]); ++i) {
            Serial.printf("%d",r.weekday[i]);
        }
            Serial.printf("\r\n");

        for (int i = 0; i < 720; i++) {
            horario h = r.horarios[i];
            Serial.println(String(h.start) + " - " + String(h.end));
        }
        Serial.printf("\r\n\r\n");
        delay(200);
    }
}
void printRotina(const routine &r)  {    
    Serial.printf("%s\r\n",r.id);
    for (size_t i = 0; i < sizeof(r.weekday) / sizeof(r.weekday[0]); ++i) {
        Serial.printf("%d",r.weekday[i]);
    }
        Serial.printf("\r\n");

    for (int i = 0; i < 720; i++) {
        horario h = r.horarios[i];
        Serial.println(String(h.start) + " - " + String(h.end));
    }
    Serial.printf("\r\n\r\n");
    delay(200);
}


JsonDocument  AquariumServices::getRoutines(){
    SetupDevice _setupDevice(_aquarium);
    vector<routine> data = _setupDevice.read<routine>("/rotinas.bin");
    
    return routineVectorToJSON(data);
}


void AquariumServices::setRoutines(routine& r){
    SetupDevice _setupDevice(_aquarium);
    vector<routine> routines = _setupDevice.read<routine>("/rotinas.bin");
    size_t i = 0;
    bool found = false;
    while (i < routines.size()) {
        if(strcmp(r.id, routines[i].id) == 0){
            routines[i] = r;
            found = true;
            break;
        }
        i++;
    }
    if(found)
        _setupDevice.write<routine>(routines, "/rotinas.bin");
}

void AquariumServices::removeRoutine(char id[37]){
    SetupDevice _setupDevice(_aquarium);
    vector<routine> routines = _setupDevice.read<routine>("/rotinas.bin");
    size_t i = 0;
    bool found = false;
    while (i < routines.size()) {
        if(strcmp(id, routines[i].id) == 0){
            routines.erase(routines.begin() + i);
            found = true;
            break;
        }
        i++;
    }
    if(found)
        _setupDevice.write<routine>(routines, "/rotinas.bin");
}
void AquariumServices::addRoutines(routine& r){
    SetupDevice _setupDevice(_aquarium);
    vector<routine> data = _setupDevice.read<routine>("/rotinas.bin");

    data.push_back(r);
    _setupDevice.write<routine>(data, "/rotinas.bin");

    data.clear();
}
void AquariumServices::updateConfiguration(int min_temperature, int max_temperature, int ph_min, int ph_max, int dosagem, int tempo_reaplicacao){
    if (!_aquarium->setHeaterAlarm(min_temperature, max_temperature))
    {
      throw std::runtime_error("Falha ao definir intervalo de temperatura, tente novamente");
    }

    if (!_aquarium->setPhAlarm(ph_min, ph_max))
    {
      throw std::runtime_error("Falha ao definir intervalo de ph, tente novamente");
    }
}
JsonDocument  AquariumServices::handlerWaterPump() {
    JsonDocument  doc;

    SetupDevice _setupDevice(_aquarium);
    vector<routine> rotinas = _setupDevice.read<routine>("/rotinas.bin");
    
    for (const auto& r : rotinas) {
        tm now = clockUTC.getDateTime();
        
        if(r.weekday[now.tm_wday]){
            for (int i = 0; i < 720; i++) {              
                horario h = r.horarios[i];

                if(h.start == 1441 || h.end == 1441)
                    continue;
                
                if((now.tm_hour * 60 + now.tm_min) >= h.start  && (now.tm_hour * 60 + now.tm_min) < h.end){
                    _aquarium->setWaterPumpStatus(HIGH);
                    doc["status_pump"] = true;
                    doc["duracao"] = (h.end - h.start) * 60;
                    doc["tempo_restante"] = (h.end * 60) - (now.tm_hour *120 + now.tm_min * 60 + now.tm_sec);
                    return doc;
                }
            }
        }
    }

    _aquarium->setWaterPumpStatus(LOW);
    doc["status_pump"] = false;
    doc["duracao"] = -1;
    doc["tempo_restante"] = -1;
    rotinas.clear();
    return doc;
}

void AquariumServices::handlerWaterPump(Date now) {
    try
    {
    SetupDevice _setupDevice(_aquarium);
        vector<routine> rotinas = _setupDevice.read<routine>("/rotinas.bin");
        for (const auto& r : rotinas) {
            if(r.weekday[now.day_of_week]){
                for (int i = 0; i < 720; i++) {
                    horario h = r.horarios[i];

                    if(h.start == 1441 || h.end == 1441)
                        continue;
                        
                    if((now.hour * 60 + now.minute) >= h.start  && (now.hour * 60 + now.minute) < h.end){
                        _aquarium->setWaterPumpStatus(HIGH);
                        return;
                    }
                    _aquarium->setWaterPumpStatus(LOW);
                }
            }
        }
        rotinas.clear();
    }
    catch (const std::exception& e)
    {
        String err = e.what();
        Serial.println("description: " + err);
    }
}

#endif