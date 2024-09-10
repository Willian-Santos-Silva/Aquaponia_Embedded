#ifndef AQUARIUM_SERVICES_H
#define AQUARIUM_SERVICES_H

#include <vector>

#include <ArduinoJson.h>
#include "Clock/Date.h"
#include "Aquarium.h"
#include "Clock/Clock.h"
#include "Clock/Date.h"
#include "Base/memory.h"
using namespace std;

class AquariumServices
{
private:
    Aquarium* _aquarium;
    Clock clockUTC;
    Memory memory;
    aplicacoes applySolution(const vector<aplicacoes>& aplicacaoList, Aquarium::solution so);
    aplicacoes applyRiseSolution(const aplicacoes& lastAplicacao, double deltaPh, int acumuladoAplicado);
    aplicacoes applyLowerSolution(const aplicacoes& lastAplicacao, double deltaPh, int acumuladoAplicado);
public:
    AquariumServices(Aquarium *aquarium);
    ~AquariumServices();

    void updateConfiguration(int min_temperature, int max_temperature, int ph_min, int ph_max, int dosagem, int tempo_reaplicacao);
    DynamicJsonDocument getSystemInformation();
    DynamicJsonDocument getRoutines();
    DynamicJsonDocument getConfiguration();
    void setRoutines(routine r);
    void addRoutines(routine r);
    void removeRoutine(char id[36]);
    void controlPeristaultic();
    DynamicJsonDocument handlerWaterPump();
};

AquariumServices::AquariumServices(Aquarium *aquarium) : _aquarium(aquarium)
{
}

AquariumServices::~AquariumServices()
{
}

DynamicJsonDocument AquariumServices::getSystemInformation()
{
    DynamicJsonDocument doc(5028);
    doc["temperatura"] = _aquarium->readTemperature();
    doc["rtc"] = clockUTC.getDateTimeString();
    doc["ph"] = _aquarium->getPh();
    doc["ntu"] = _aquarium->getTurbidity();
    doc["status_heater"] = _aquarium->getHeaterStatus();
    doc["status_water_pump"] = _aquarium->getWaterPumpStatus();
    
    return doc;
}
void AquariumServices::controlPeristaultic() {
    vector<aplicacoes> aplicacaoList = _aquarium->readAplicacao();

    aplicacoes applyRise = applySolution(aplicacaoList, _aquarium->SOLUTION_RAISER);
    aplicacoes applyLower = applySolution(aplicacaoList, _aquarium->SOLUTION_LOWER);
    
    if(aplicacaoList.size() < 10 & applyRise.ml > 0.0){
        Serial.printf("Dosagem Raise: %lu", applyRise.ml);
        aplicacaoList.push_back(applyRise);
        _aquarium->addAplicacao(aplicacaoList);
    }

    if(aplicacaoList.size() < 10 & applyLower.ml > 0.0){
        Serial.printf("Dosagem Lower: %lu", applyRise.ml);
        aplicacaoList.push_back(applyLower);
        _aquarium->addAplicacao(aplicacaoList);
    }

}

aplicacoes AquariumServices::applySolution(const vector<aplicacoes>& aplicacaoList, Aquarium::solution solution) {
    aplicacoes ultimaAplicacao;
    aplicacoes primeiraAplicacao;

    struct tm *timeUltimaAplicacao;
    time_t interval;
    bool hasLast = false;


    double deltaPh;
    int acumuladoAplicado = 0;

    for (uint32_t i = (aplicacaoList.size() - 1); i >= 0; i--) {
        aplicacoes aplicacao = aplicacaoList[i];

        if(aplicacao.type != solution)
            continue;
        primeiraAplicacao = aplicacaoList[i];
            
        if(!hasLast){
            timeUltimaAplicacao = localtime(&ultimaAplicacao.dataAplicacao);
            interval = (long)ultimaAplicacao.dataAplicacao - DEFAULT_TIME_DELAY_PH;
            hasLast = true;
        }

        if(aplicacao.dataAplicacao < interval)
            break;

        acumuladoAplicado += aplicacao.ml;
    }

    deltaPh = primeiraAplicacao.deltaPh + primeiraAplicacao.deltaPh;
    
    aplicacoes aplicacao;
    

    if(solution == _aquarium->SOLUTION_LOWER){
        aplicacao = applyLowerSolution(ultimaAplicacao, deltaPh, acumuladoAplicado);
        return aplicacao;
    }

    if(solution == _aquarium->SOLUTION_RAISER){
        aplicacao = applyRiseSolution(ultimaAplicacao, deltaPh, acumuladoAplicado);
        return aplicacao;
    }

    return aplicacao;
}

aplicacoes AquariumServices::applyRiseSolution(const aplicacoes& lastAplicacao, double deltaPh, int acumuladoAplicado) {
    aplicacoes aplicacao;
    aplicacao.ml = acumuladoAplicado;

    double ml_s = 1.0 / 10.0;

    int initialPh = _aquarium->getPh();
    int goal = _aquarium->getMinPh() + (_aquarium->getMaxPh() - _aquarium->getMinPh()) / 2;
    
    while(acumuladoAplicado < 7){
        int ph = _aquarium->getPh();

        if (ph >= _aquarium->getMinPh())
        {
            break;
        }
        int maxBufferSolutionMl = AQUARIUM_VOLUME_L * (DOSAGE_RAISE_SOLUTION_M3_L / 1000);
        _aquarium->setPeristaulticStatus(ml_s, _aquarium->SOLUTION_RAISER);

        acumuladoAplicado += ml_s;
        aplicacao.ml += ml_s;
        vTaskDelay(500 / portTICK_PERIOD_MS);
        deltaPh = ph - initialPh;
    }
    return aplicacao;
}

aplicacoes AquariumServices::applyLowerSolution(const aplicacoes& lastAplicacao, double deltaPh, int acumuladoAplicado) {
    aplicacoes aplicacao;
    aplicacao.ml = acumuladoAplicado;

    double ml_s = 1.0 / 10.0;

    int initialPh = _aquarium->getPh();
    int goal = _aquarium->getMinPh() + (_aquarium->getMaxPh() - _aquarium->getMinPh()) / 2;
    
    while(acumuladoAplicado < 7){
        int ph = _aquarium->getPh();

        if (ph <= _aquarium->getMaxPh())
        {
            break;
        }

        int maxAlkalineSolutionMl = AQUARIUM_VOLUME_L * (DOSAGE_LOWER_SOLUTION_M3_L / 1000);
        _aquarium->setPeristaulticStatus(ml_s, _aquarium->SOLUTION_LOWER);

        acumuladoAplicado += ml_s;
        aplicacao.ml += ml_s;
        vTaskDelay(500 / portTICK_PERIOD_MS);
        deltaPh = ph - initialPh;
    }
    aplicacao.deltaPh = deltaPh;
    return aplicacao;
}


DynamicJsonDocument AquariumServices::getConfiguration(){
    DynamicJsonDocument doc(5028);

    doc["min_temperature"] = _aquarium->getMinTemperature();
    doc["max_temperature"] = _aquarium->getMaxTemperature();
    doc["min_ph"] = _aquarium->getMinPh();
    doc["max_ph"] = _aquarium->getMaxPh();
    doc["ppm"] = _aquarium->getPPM();
    // doc["rtc"] = clockUTC.getDateTime().getFullDate();

    return doc;
}
DynamicJsonDocument AquariumServices::getRoutines(){
    DynamicJsonDocument doc(35000);
    JsonArray dataArray = doc.to<JsonArray>();

    vector<routine> data = _aquarium->readRoutine();    
    for (const auto& r : data) {
        JsonObject rotina = dataArray.createNestedObject();
        rotina["id"] = r.id;
        JsonArray weekdays = rotina.createNestedArray("WeekDays");
        for (size_t i = 0; i < sizeof(r.weekday) / sizeof(r.weekday[0]); ++i) {
            weekdays.add(r.weekday[i]);
        }

        JsonArray horarios = rotina.createNestedArray("horarios");
        for (const auto& h : r.horarios) {
            JsonObject horario = horarios.createNestedObject();
            horario["start"] = h.start;
            horario["end"] = h.end;
        }
    }

    data.clear();
  
    return doc;
}

void AquariumServices::setRoutines(routine r){
    vector<routine> routines = _aquarium->readRoutine();
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
        _aquarium->writeRoutine(routines);
}

void AquariumServices::removeRoutine(char id[37]){
    vector<routine> routines = _aquarium->readRoutine();
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
        _aquarium->writeRoutine(routines);
}
void AquariumServices::addRoutines(routine r){
    vector<routine> routines =_aquarium->readRoutine();
    routines.push_back(r);
  _aquarium->writeRoutine(routines);
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
DynamicJsonDocument AquariumServices::handlerWaterPump() {
    DynamicJsonDocument doc(1000);

    vector<routine> rotinas = _aquarium->readRoutine();
    if(rotinas.size() == 0){
        _aquarium->setWaterPumpStatus(LOW);
        
        doc["status_pump"] = false;
        doc["duracao"] = -1;
        doc["tempo_restante"] = -1;
        return doc;
    }
    
    for (const auto& routine : rotinas) {
        tm now = clockUTC.getDateTime();
        
        if(routine.weekday[now.tm_wday]){
            for (const auto& h : routine.horarios) {
                if((now.tm_hour * 60 + now.tm_min) >= h.start  && (now.tm_hour * 60 + now.tm_min) < h.end){
                    _aquarium->setWaterPumpStatus(HIGH);
                    doc["status_pump"] = true;
                    doc["duracao"] = (h.end - h.start) * 60;
                    doc["tempo_restante"] = (h.end * 60) - (now.tm_hour *120 + now.tm_min * 60 + now.tm_sec);
                    return doc;
                }
                _aquarium->setWaterPumpStatus(LOW);
            }
        }
    }
    doc["status_pump"] = false;
    doc["duracao"] = -1;
    doc["tempo_restante"] = -1;
    rotinas.clear();
    return doc;
}


#endif