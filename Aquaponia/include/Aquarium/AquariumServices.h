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

class AquariumServices
{
private:
    Aquarium* _aquarium;
    SetupDevice* _setupDevice;
    Clock clockUTC;

    aplicacoes applySolution(const vector<aplicacoes>& aplicacaoList, Aquarium::solution so);
    aplicacoes applyRiseSolution(double deltaPh, double acumuladoAplicado);
    aplicacoes applyLowerSolution(double deltaPh, double acumuladoAplicado);
    void handlerWaterPump(Date now);
    
public:
    AquariumServices(Aquarium *aquarium, SetupDevice* setupDevice);
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

AquariumServices::AquariumServices(Aquarium *aquarium, SetupDevice* setupDevice) : _aquarium(aquarium), _setupDevice(setupDevice)
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


const char* printData(time_t *data) {

    struct tm *timeinfo = gmtime(data);

    static char dateTimeStr[30];
    snprintf(dateTimeStr, sizeof(dateTimeStr), "%02d/%02d/%04d %02d:%02d:%02d",
        timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900,
        timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    // Serial.printf("DATA: %s\r\n\r\n", dateTimeStr);
    return dateTimeStr;
}


void AquariumServices::controlPeristaultic() {
    vector<aplicacoes> aplicacaoList = _setupDevice->readAplicacao();

    aplicacoes applyRaiser = applySolution(aplicacaoList, _aquarium->SOLUTION_RAISER);
    

    if(applyRaiser.ml > 0.0) {
        Serial.printf("Dosagem Raiser: %lf\r\n\r\n", applyRaiser.ml);

        aplicacaoList.push_back(applyRaiser);

        if(aplicacaoList.size() > 10)
            aplicacaoList.erase(aplicacaoList.begin());

        _setupDevice->addAplicacao(aplicacaoList);
    }


    aplicacoes applyLower = applySolution(aplicacaoList, _aquarium->SOLUTION_LOWER);
    if(applyLower.ml > 0.0){
        Serial.printf("Dosagem Lower: %lf\r\n\r\n", applyLower.ml);

        aplicacaoList.push_back(applyLower);
        
        if(aplicacaoList.size() > 10)
            aplicacaoList.erase(aplicacaoList.begin());
            
        _setupDevice->addAplicacao(aplicacaoList);
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
        Serial.print("================================= Dosar ================================= \r\n");


    tm now = clockUTC.getDateTime();
    tm *nowp = &now;
    time_t timestamp = mktime(nowp); 
        Serial.printf("DATA: %s\r\n", printData(&timestamp));    
    // delete nowp;

    for (uint32_t i = aplicacaoList.size(); i > 0; i--) {
        aplicacoes aplicacao = aplicacaoList[i - 1];

        if(aplicacao.type != solution)
            continue;

        Serial.printf("TIPO: %s\r\n", aplicacao.type ==  Aquarium::SOLUTION_RAISER ? "RAISER" : "LOWER");    
        Serial.printf("DOSADO: %lf\r\n", aplicacao.ml);
        Serial.printf("DELTA: %lf\r\n", aplicacao.deltaPh);    
        Serial.printf("DATA: %s\r\n", printData(&aplicacao.dataAplicacao));    

        primeiraAplicacao = aplicacao;
            
        if(!hasLast){
            ultimaAplicacao = aplicacao;
            hasLast = true;
        }
        Serial.printf("interval: %i\r\n", ((long)ultimaAplicacao.dataAplicacao - (long)aplicacao.dataAplicacao));        
        Serial.printf("interval: %i\r\n\r\n", ((long)ultimaAplicacao.dataAplicacao - (long)aplicacao.dataAplicacao));        

        if((long)ultimaAplicacao.dataAplicacao - (long)aplicacao.dataAplicacao >= DEFAULT_TIME_DELAY_PH
         || ((long)timestamp - (long)ultimaAplicacao.dataAplicacao) >=  DEFAULT_TIME_DELAY_PH)
            break;

        acumuladoAplicado += aplicacao.ml;
    }

    Serial.print("======================================================================= \r\n\r\n");

    if(hasLast){
        Serial.printf("DATA ULTIMA: %s\r\n", printData(&ultimaAplicacao.dataAplicacao));
    }


    // deltaPh = primeiraAplicacao.deltaPh + ultimaAplicacao.deltaPh;
    // Serial.printf("Delta: %lf\r\n", deltaPh);
    Serial.printf("Acumulado: %lf\r\n\r\n", acumuladoAplicado);
    
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

    Serial.printf("maxBufferSolutionMl: %lf\r\n\r\n", maxBufferSolutionMl);

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

    Serial.printf("maxAlkalineSolutionMl: %lf\r\n\r\n", maxAlkalineSolutionMl);

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

    Serial.printf("APLICADO: %lf\r\n", aplicacao.ml);


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

    vector<routine> data = _setupDevice->readRoutine();    
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
    vector<routine> routines = _setupDevice->readRoutine();
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
        _setupDevice->writeRoutine(routines);
}

void AquariumServices::removeRoutine(char id[37]){
    vector<routine> routines = _setupDevice->readRoutine();
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
        _setupDevice->writeRoutine(routines);
}
void AquariumServices::addRoutines(routine r){
    vector<routine> routines =_setupDevice->readRoutine();
    routines.push_back(r);
    _setupDevice->writeRoutine(routines);
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

    vector<routine> rotinas = _setupDevice->readRoutine();
    
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
        vector<routine> rotinas = _setupDevice->readRoutine();
        for (const auto& routine : rotinas) {
            if(routine.weekday[now.day_of_week]){
                for (const auto& h : routine.horarios) {
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