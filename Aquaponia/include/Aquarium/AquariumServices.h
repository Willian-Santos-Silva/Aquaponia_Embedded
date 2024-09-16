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

void AquariumServices::controlPeristaultic() {
    vector<aplicacoes> aplicacaoList = _setupDevice->readAplicacao();


    aplicacoes applyRaiser = applySolution(aplicacaoList, _aquarium->SOLUTION_RAISER);
    aplicacoes applyLower = applySolution(aplicacaoList, _aquarium->SOLUTION_LOWER);
    
    if(aplicacaoList.size() < 10 && applyRaiser.ml > 0.0){
        Serial.printf("Dosagem Raiser: %lf\r\n\r\n", applyRaiser.ml);

        struct tm *timeinfo = gmtime(&applyRaiser.dataAplicacao);
    
        // static char dateTimeStr[30];
        // snprintf(dateTimeStr, sizeof(dateTimeStr), "%02d/%02d/%04d %02d:%02d:%02d",
        //     timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900,
        //     timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);


        // Serial.printf("DELTA: %lf\r\n", applyRaiser.deltaPh);
        // Serial.printf("TYPE: %s\r\n", (applyRaiser.type == Aquarium::SOLUTION_LOWER ? "SOLUTION_LOWER" : "SOLUTION_RAISER"));
        // Serial.printf("ML: %lf\r\n", applyRaiser.ml);
        // Serial.printf("DATA: %s\r\n\r\n", dateTimeStr);


        aplicacaoList.push_back(applyRaiser);
        _setupDevice->addAplicacao(aplicacaoList);
    }

    if(aplicacaoList.size() < 10 && applyLower.ml > 0.0){
        Serial.printf("Dosagem Lower: %lf\r\n\r\n", applyLower.ml);


        // struct tm *timeinfo = gmtime(&applyLower.dataAplicacao);
    
        // static char dateTimeStr[30];
        // snprintf(dateTimeStr, sizeof(dateTimeStr), "%02d/%02d/%04d %02d:%02d:%02d",
        //     timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900,
        //     timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);


        // Serial.printf("DELTA: %lf\r\n", applyLower.deltaPh);
        // Serial.printf("TYPE: %s\r\n", (applyLower.type == Aquarium::SOLUTION_LOWER ? "SOLUTION_LOWER" : "SOLUTION_RAISER"));
        // Serial.printf("ML: %lf\r\n", applyLower.ml);
        // Serial.printf("DATA: %s\r\n\r\n", dateTimeStr);


        aplicacaoList.push_back(applyLower);
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

    for (uint32_t i = aplicacaoList.size(); i > 0; i--) {
        aplicacoes aplicacao = aplicacaoList[i - 1];

        if(aplicacao.type != solution)
            continue;

        primeiraAplicacao = aplicacaoList[i - 1];
            
        if(!hasLast){
            timeUltimaAplicacao = localtime(&ultimaAplicacao.dataAplicacao);
            dataProximaAplicacao = (long)ultimaAplicacao.dataAplicacao + DEFAULT_TIME_DELAY_PH;
            hasLast = true;
        }

        if(aplicacao.dataAplicacao < dataProximaAplicacao)
            break;

        acumuladoAplicado += aplicacao.ml;
    }
    deltaPh = primeiraAplicacao.deltaPh + ultimaAplicacao.deltaPh;
    
    aplicacoes aplicacao;

    if(solution == Aquarium::SOLUTION_LOWER){
        aplicacao = applyLowerSolution(deltaPh, acumuladoAplicado);
        aplicacao.type = Aquarium::SOLUTION_LOWER;
        return aplicacao;
    }

    if(solution == Aquarium::SOLUTION_RAISER){
        aplicacao = applyRiseSolution(deltaPh, acumuladoAplicado);
        aplicacao.type = Aquarium::SOLUTION_RAISER;
        return aplicacao;
    }

    return aplicacao;
}

aplicacoes AquariumServices::applyRiseSolution(double deltaPh, double acumuladoAplicado) {
    aplicacoes aplicacao;
    aplicacao.ml = acumuladoAplicado;

    double ml_s = 1;

    float initialPh = _aquarium->getPh();
    int goal = _aquarium->getMinPh() + (_aquarium->getMaxPh() - _aquarium->getMinPh()) / 2;
    
    double maxBufferSolutionMl = AQUARIUM_VOLUME_L * (DOSAGE_RAISE_SOLUTION_M3_L / 1000.0);
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

    return aplicacao;
}

aplicacoes AquariumServices::applyLowerSolution(double deltaPh, double acumuladoAplicado) {
    aplicacoes aplicacao;
    aplicacao.ml = acumuladoAplicado;

    double ml_s = 1;

    float initialPh = _aquarium->getPh();
    int goal = _aquarium->getMinPh() + (_aquarium->getMaxPh() - _aquarium->getMinPh()) / 2;
    
    int maxAlkalineSolutionMl = AQUARIUM_VOLUME_L * (DOSAGE_LOWER_SOLUTION_M3_L / 1000);
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