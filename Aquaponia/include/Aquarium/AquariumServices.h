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
  }
  return doc;
}

class AquariumServices
{
private:
    Aquarium* _aquarium;
    Clock clockUTC;
    SetupDevice* _setupDevice;


public:
    AquariumServices(Aquarium *aquarium);
    ~AquariumServices();

    void updateConfiguration(int min_temperature, int max_temperature, int ph_min, int ph_max, int dosagem_solucao_acida, int dosagem_solucao_base, long tempo_reaplicacao);
    JsonDocument  getSystemInformation();
    JsonDocument  getHistPh();
    JsonDocument  getHistTemp();
    JsonDocument  getRoutines();
    JsonDocument  getConfiguration();
    void setRoutines(routine* r);
    void addRoutines(routine* r);
    void removeRoutine(char id[36]);
    void controlPeristaultic();

    aplicacoes applySolution(const vector<aplicacoes>& aplicacaoList, Aquarium::solution so);
    aplicacoes applyRiseSolution(double deltaPh, double acumuladoAplicado);
    aplicacoes applyLowerSolution(double deltaPh, double acumuladoAplicado);
    JsonDocument  handlerWaterPump();
};

AquariumServices::AquariumServices(Aquarium *aquarium) : _aquarium(aquarium)
{
    _setupDevice = new SetupDevice(aquarium);
}

AquariumServices::~AquariumServices()
{
}

JsonDocument  AquariumServices::getSystemInformation()
{
    JsonDocument  doc;
    doc["temperatura"] = _aquarium->readTemperature();
    doc["rtc"] = clockUTC.getTimestamp();
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

    return dateTimeStr;
}

time_t tmToTimestamp(tm * time) {
    return mktime(time); 
}


JsonDocument  AquariumServices::getHistPh(){
    JsonDocument  doc;
    doc["min_ph"] = _aquarium->getMinPh();
    doc["max_ph"] = _aquarium->getMaxPh();

    JsonArray dataArray = doc["history"].to<JsonArray>();

    vector<historicoPh> list = _setupDevice->read<historicoPh>("/histPh.bin");

    for (const auto& data : list) {
        JsonObject dataObj = dataArray.add<JsonObject>();
        dataObj["ph"] = data.ph;
        dataObj["timestamp"] = data.time;
    }

    return doc;
}

JsonDocument  AquariumServices::getHistTemp(){
    JsonDocument  doc;
    doc["min_temperatura"] = _aquarium->getMinTemperature();
    doc["max_temperatura"] = _aquarium->getMaxTemperature();

    JsonArray dataArray = doc["history"].to<JsonArray>();

    vector<historicoTemperatura> list = _setupDevice->read<historicoTemperatura>("/histTemp.bin");

    for (const auto& data : list) {
        JsonObject dataObj = dataArray.add<JsonObject>();
        dataObj["temperatura"] = data.temperatura;
        dataObj["timestamp"] = data.time;
    }

  
    return doc;
}


void AquariumServices::controlPeristaultic() {
    vector<aplicacoes> aplicacaoList = _setupDevice->read<aplicacoes>("/pump.bin");

    aplicacoes applyRaiser = applySolution(aplicacaoList, _aquarium->SOLUTION_RAISER);

    if(applyRaiser.ml > 0.0) {
        log_e("Dosagem Raiser: %lf", applyRaiser.ml);

        aplicacaoList.push_back(applyRaiser);

        if(aplicacaoList.size() > 10)
            aplicacaoList.erase(aplicacaoList.begin());

        _setupDevice->write<aplicacoes>(aplicacaoList, "/pump.bin");
    }


    aplicacoes applyLower = applySolution(aplicacaoList, _aquarium->SOLUTION_LOWER);
    if(applyLower.ml > 0.0){
        log_e("Dosagem Lower: %lf", applyLower.ml);

        aplicacaoList.push_back(applyLower);
        
        if(aplicacaoList.size() > 10)
            aplicacaoList.erase(aplicacaoList.begin());
            
        _setupDevice->write<aplicacoes>(aplicacaoList, "/pump.bin");
    }

    aplicacaoList.clear();
}

aplicacoes AquariumServices::applySolution(const vector<aplicacoes>& aplicacaoList, Aquarium::solution solution) {
    aplicacoes ultimaAplicacao;
    aplicacoes primeiraAplicacao;

    tm *timeUltimaAplicacao;
    time_t dataProximaAplicacao;
    bool hasLast = false;


    double deltaPh;
    double acumuladoAplicado = 0;

    time_t timestamp = clockUTC.getTimestamp(); 

    for (uint32_t i = aplicacaoList.size(); i > 0; i--) {
        aplicacoes aplicacao = aplicacaoList[i - 1];

        if(aplicacao.type != solution)
            continue;

        primeiraAplicacao = aplicacao;
            
        if(!hasLast){
            ultimaAplicacao = aplicacao;
            hasLast = true;
        }

        if((long)ultimaAplicacao.dataAplicacao - (long)aplicacao.dataAplicacao >= _aquarium->getTempoReaplicacao()
         || ((long)timestamp - (long)ultimaAplicacao.dataAplicacao) >=  _aquarium->getTempoReaplicacao())
            break;

        acumuladoAplicado += aplicacao.ml;
    }

    if(hasLast){
        log_e("DATA ULTIMA: %s", printData(&ultimaAplicacao.dataAplicacao));
    }
    // free(timeUltimaAplicacao);
    
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

    double initialPh = _aquarium->getPh();
    int goal = _aquarium->getMinPh() + (_aquarium->getMaxPh() - _aquarium->getMinPh()) / 2;
    
    double maxBufferSolutionMl = AQUARIUM_VOLUME_L / static_cast<double>(_aquarium->getRaiserSolutionDosage());

    while ((acumuladoAplicado + aplicacao.ml + ml_s) <= maxBufferSolutionMl) {
        double ph = _aquarium->getPh();

        if (ph >= _aquarium->getMinPh())
        {
            break;
        }

        _aquarium->setPeristaulticStatus(ml_s, _aquarium->SOLUTION_RAISER);
        
        aplicacao.ml += ml_s;
        deltaPh = ph - initialPh;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return aplicacao;
}

aplicacoes AquariumServices::applyLowerSolution(double deltaPh, double acumuladoAplicado) {
    aplicacoes aplicacao;
    aplicacao.ml = 0.0;
    double ml_s = 1.0;

    double initialPh = _aquarium->getPh();
    int goal = _aquarium->getMinPh() + (_aquarium->getMaxPh() - _aquarium->getMinPh()) / 2;
    
    double maxAlkalineSolutionMl = AQUARIUM_VOLUME_L / static_cast<double>(_aquarium->getLowerSolutionDosage());

    while ((acumuladoAplicado + aplicacao.ml + ml_s) <= maxAlkalineSolutionMl) {
        double ph = _aquarium->getPh();

        if (ph <= _aquarium->getMaxPh())
        {
            break;
        }

        _aquarium->setPeristaulticStatus(ml_s, _aquarium->SOLUTION_LOWER);
        
        aplicacao.ml += ml_s;
        deltaPh = ph - initialPh;

        vTaskDelay(pdMS_TO_TICKS(100));
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
    doc["dosagem_solucao_acida"] = _aquarium->getLowerSolutionDosage();
    doc["dosagem_solucao_base"] = _aquarium->getRaiserSolutionDosage();
    doc["tempo_reaplicacao"] = _aquarium->getTempoReaplicacao();
    doc["rtc"] = clockUTC.getTimestamp();

    return doc;
}

void printRotinas(vector<routine> &data){    
    for (const auto& r : data) {
        log_e("%s",r.id);
        for (size_t i = 0; i < sizeof(r.weekday) / sizeof(r.weekday[0]); ++i) {
            log_e("%d",r.weekday[i]);
        }

        for (int i = 0; i < 720; i++) {
            horario h = r.horarios[i];
            Serial.println(String(h.start) + " - " + String(h.end));
        }
        
        delay(200);
    }
}
void printRotina(const routine &r)  {    
    log_e("%s",r.id);
    for (size_t i = 0; i < sizeof(r.weekday) / sizeof(r.weekday[0]); ++i) {
        log_e("%d",r.weekday[i]);
    }

    for (int i = 0; i < 720; i++) {
        horario h = r.horarios[i];
        Serial.println(String(h.start) + " - " + String(h.end));
    }

    delay(200);
}


JsonDocument  AquariumServices::getRoutines(){
    vector<routine> data = _setupDevice->read<routine>("/rotinas.bin");
    
    return routineVectorToJSON(data);
}


void AquariumServices::setRoutines(routine* r){
    vector<routine> routines = _setupDevice->read<routine>("/rotinas.bin");
    size_t i = 0;
    bool found = false;
    while (i < routines.size()) {
        if(strcmp(r->id, routines[i].id) == 0){
            routines[i] = *r;
            found = true;
            break;
        }
        i++;
    }
    if(found)
        _setupDevice->write<routine>(routines, "/rotinas.bin");
}

void AquariumServices::removeRoutine(char id[37]){
    vector<routine> routines = _setupDevice->read<routine>("/rotinas.bin");
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
        _setupDevice->write<routine>(routines, "/rotinas.bin");
}
void AquariumServices::addRoutines(routine* r){
    vector<routine> data = _setupDevice->read<routine>("/rotinas.bin");

    data.push_back(*r);
    _setupDevice->write<routine>(data, "/rotinas.bin");

    data.clear();
}
void AquariumServices::updateConfiguration(int min_temperature, int max_temperature, int ph_min, int ph_max, int dosagem_solucao_acida, int dosagem_solucao_base, long tempo_reaplicacao){
    if (!_aquarium->setHeaterAlarm(min_temperature, max_temperature))
    {
        throw std::runtime_error("Falha ao definir intervalo de temperatura, tente novamente");
    }

    if (!_aquarium->setPhAlarm(ph_min, ph_max))
    {
        throw std::runtime_error("Falha ao definir intervalo de ph, tente novamente");
    }
    if (!_aquarium->setLowerSolutionDosage(dosagem_solucao_acida))
    {
        throw std::runtime_error("Falha ao definir a dosagem da solução, tente novamente");
    }
    
    if (!_aquarium->setRaiserSolutionDosage(dosagem_solucao_base))
    {
        throw std::runtime_error("Falha ao definir a dosagem da solução, tente novamente");
    }

    
    Serial.printf("\r\nSETADOS\r\n");

    
    Serial.printf("min_temperature: %i\r\n",        _aquarium->getMinTemperature());
    Serial.printf("max_temperature: %i\r\n",        _aquarium->getMaxTemperature());
    Serial.printf("min_ph: %i\r\n",                 _aquarium->getMinPh());
    Serial.printf("max_ph: %i\r\n",                 _aquarium->getMaxPh());
    Serial.printf("dosagem_solucao_acida: %i\r\n",  _aquarium->getLowerSolutionDosage());
    Serial.println(_aquarium->getRaiserSolutionDosage());
    Serial.printf("tempo_reaplicacao: %ld\r\n",     _aquarium->getTempoReaplicacao());

    Serial.printf("\r\nSETADOS\r\n");

}
JsonDocument  AquariumServices::handlerWaterPump() {
    JsonDocument  doc;

    vector<routine> rotinas = _setupDevice->read<routine>("/rotinas.bin");
    
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
#endif