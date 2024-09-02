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
    // doc["rtc"] = clockUTC.getDateTime().getFullDate();
    doc["ph"] = _aquarium->getPh();
    doc["ph_v"] = _aquarium->getTensao();
    // doc["ip"] = localNetwork.GetIp().c_str();
    doc["status_heater"] = _aquarium->getHeaterStatus();
    doc["status_water_pump"] = _aquarium->getWaterPumpStatus();
    //remaining_time_irrigation 
    
    return doc;
}
void AquariumServices::controlPeristaultic(){
    int ph = _aquarium->getPh();
    int goal = _aquarium->getMinPh() + (_aquarium->getMaxPh() - _aquarium->getMinPh()) / 2;

    memory.write<long>(ADDRESS_LAST_APPLICATION_ACID_BUFFER_PH, DEFAULT_TIME_DELAY_PH);

    int ml_s = 1 / 1000;

    if (ph <= _aquarium->getMinPh())
    {
      int maxBufferSolutionMl = AQUARIUM_VOLUME_L * (DOSAGE_RAISE_SOLUTION_M3_L / 1000);

      _aquarium->setPeristaulticStatus(ml_s, _aquarium->SOLUTION_RAISER);
    }

    if (ph >= _aquarium->getMaxPh())
    {
      int maxAlkalineSolutionMl = AQUARIUM_VOLUME_L * (DOSAGE_LOWER_SOLUTION_M3_L / 1000);

      _aquarium->setPeristaulticStatus(ml_s, _aquarium->SOLUTION_LOWER);
    }
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
    DynamicJsonDocument doc(5028);


    vector<routine> rotinas = _aquarium->readRoutine();
    for (const auto& routine : rotinas) {
        Date now = clockUTC.getDateTime();
        
        if(routine.weekday[now.day_of_week]){
            for (const auto& h : routine.horarios) {
                if((now.hour * 60 + now.minute) >= h.start  && (now.hour * 60 + now.minute) < h.end){
                    _aquarium->setWaterPumpStatus(HIGH);
                    rotinas.clear();
                    doc["status_pump"] = true;
                    doc["duracao"] = h.end - h.start;
                    doc["tempo_restante"] = h.end - (now.hour * 60 + now.minute);
                    return doc;
                }
                _aquarium->setWaterPumpStatus(LOW);
                doc["status_pump"] = false;
                doc["duracao"] = -1;
                doc["tempo_restante"] = -1;
            }
        }
    }
    rotinas.clear();
    return doc;
}


#endif