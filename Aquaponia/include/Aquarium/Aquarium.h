#ifndef AQUARIUM_H
#define AQUARIUM_H

#include <OneWire.h>
#include <DS18B20.h>
#include <vector>

#include "Base/config.h"
#include "Base/memory.h"
#include "Clock/Clock.h"
#include "Clock/Date.h"

#include "FS.h"
#include "SPIFFS.h"
#include "routines.h"
#include "uuid.h"
using namespace std;

class Aquarium
{
private:
    DS18B20 ds;
    Memory _memory;

    float _temperature; 

public:
    enum solution {
      SOLUTION_LOWER,
      SOLUTION_RAISER
    };

    Aquarium() : ds(PIN_THERMOCOUPLE)
    {
    }

    void begin()
    {
        pinMode(PIN_HEATER, OUTPUT);
        pinMode(PIN_WATER_PUMP, OUTPUT);
        pinMode(PIN_PH, INPUT);

        const int pwmChannel = 0; // Canal PWM
        const int pwmFreq = 5000; // Frequência do PWM em Hz
        const int pwmResolution = 8; // Resolução do PWM (8 bits -> 0 a 255)
        ledcSetup(pwmChannel, pwmFreq, pwmResolution);
    
        pinMode(PIN_PERISTAULTIC_RAISER, OUTPUT);
        ledcAttachPin(PIN_PERISTAULTIC_RAISER, pwmChannel);

        pinMode(PIN_PERISTAULTIC_LOWER, OUTPUT);
        ledcAttachPin(PIN_PERISTAULTIC_LOWER, pwmChannel);

        
        if (_memory.readBool(ADDRESS_START)){
            Serial.println("Startado");
            return;
        }

        Serial.printf(setHeaterAlarm(MIN_AQUARIUM_TEMP, MAX_AQUARIUM_TEMP) ? "TEMPERATURAS DEFINIDAS" : "FALHA AO DEFINIR INTERVALO DE TEMPERATURA");
        Serial.printf("\n");
        Serial.printf(setPhAlarm(MIN_AQUARIUM_PH, MAX_AQUARIUM_PH) ? "PH DEFINIDO" : "FALHA AO DEFINIR INTERVALO DE PH");

        if (!SPIFFS.begin(true)) {
            throw std::runtime_error("Falha ao montar o sistema de arquivos SPIFFS");
        }
        if(SPIFFS.exists("/rotinas.bin"))
            SPIFFS.remove("/rotinas.bin");
        SPIFFS.end();
    }
    void writeRoutine(const vector<routine> &routines)
    {
        if (!SPIFFS.begin(true)) {
            throw std::runtime_error("Falha ao montar o sistema de arquivos SPIFFS");
        }
        if(SPIFFS.exists("/rotinas.bin")){
            SPIFFS.remove("/rotinas.bin");
            SPIFFS.end();
        }
        
        File file = SPIFFS.open("/rotinas.bin", FILE_WRITE);
        
        uint32_t routinesSize = routines.size();
        file.write((uint8_t *)&routinesSize, sizeof(routinesSize));

        for (const auto &r : routines) {
            file.write((uint8_t*)r.id, sizeof(r.id));
            Serial.printf("\nid: %s\n",r.id);
            file.write((uint8_t *)r.weekday, sizeof(r.weekday));


            uint32_t horariosSize = r.horarios.size();
            file.write((uint8_t *)&horariosSize, sizeof(horariosSize));

            for (const auto &h : r.horarios) {
                file.write((uint8_t *)&h, sizeof(h));
            }
        }
        file.close();
        SPIFFS.end();
    }

    vector<routine> readRoutine()
    {
        vector<routine> routines = {};

        if (!SPIFFS.begin(true)) {
            throw std::runtime_error("Falha ao montar o sistema de arquivos SPIFFS");
        }
        
        File file = SPIFFS.open("/rotinas.bin", FILE_READ);
        if (!file) {
            Serial.println("Arquivo nao existe");
            
            SPIFFS.end();
            throw std::runtime_error("Erro ao abrir o arquivo para leitura");
        }
        
        uint32_t routinesSize;
        if (file.read((uint8_t *)&routinesSize, sizeof(routinesSize)) != sizeof(routinesSize)){
            Serial.println("Erro ao ler as rotinas");
            file.close();
            SPIFFS.end();
            return {};
        }
        
        for (uint32_t i = 0; i < routinesSize; ++i) {
            routine r;
            if (file.read((uint8_t*)r.id, sizeof(r.id)) != sizeof(r.id)) {
                Serial.println("Erro ao ler o ID da rotina");
                file.close();
                SPIFFS.end();
                return {};
            }

            if (file.read((uint8_t *)&r.weekday, sizeof(r.weekday)) != sizeof(r.weekday)){
                Serial.println("Erro ao ler o weekday da rotina");
                file.close();
                return {};
            }

            uint32_t horariosSize;
            if (file.read((uint8_t *)&horariosSize, sizeof(horariosSize)) != sizeof(horariosSize)){
                Serial.println("Erro ao ler o numero de horarios da rotina");
                file.close();
                SPIFFS.end();
                return {};
            }

            for (uint32_t j = 0; j < horariosSize; ++j) {
                horario h;
                if(file.read((uint8_t *)&h, sizeof(h)) != sizeof(h)){
                    Serial.println("Erro ao ler o horario da rotina");
                    file.close();
                    return {};
                }
                r.horarios.push_back(h);
            }

            routines.push_back(r);
        }
        
        file.close();
        SPIFFS.end();
        return routines;
    }

    void updateTemperature()
    {
        if (ds.getNumberOfDevices() == 0){
            _temperature = -127.0f;
            return;
        }

        ds.selectNext();
        _temperature = ds.getTempC();
    }
    float readTemperature()
    {
        return _temperature;
    }

    float getPh()
    {    
        unsigned long int avgValue; 
        int buf[10],temp;
        
        float Vmax = 3.3;
        int Dmax = 4095;
        
        for(int i=0;i<10;i++) 
        { 
            buf[i]=analogRead(PIN_PH);
            delay(10);
        }
        for(int i=0;i<9;i++)
        {
            for(int j=i+1;j<10;j++)
            {
                if(buf[i]>buf[j])
                {
                    temp=buf[i];
                    buf[i]=buf[j];
                    buf[j]=temp;
                }
            }
        }
        avgValue=0;
        for(int i=2;i<8;i++)
            avgValue+=buf[i];

        float pHVol=(float)avgValue*Vmax/Dmax/6;
        float phValue = -3.30 * pHVol + 21.34;
        delay(20);
        
        return phValue;
    }

    float getTensao()
    {
        float Vmax = 3.3;
        int Dmax = 4095;

        float phValue = analogRead(PIN_PH);
    
        return phValue;
    }

    bool getHeaterStatus()
    {
        return digitalRead(PIN_HEATER);
    }

    bool setHeaterAlarm(int tempMin, int tempMax)
    {
        if (tempMax < tempMin || tempMax < -273 || tempMin < -273)
        {
            Serial.println("Impossivel definir essa temperatura");
            return false;
        }

        _memory.write<int>(ADDRESS_AQUARIUM_MIN_TEMPERATURE, tempMin);
        _memory.write<int>(ADDRESS_AQUARIUM_MAX_TEMPERATURE, tempMax);
        
        return getMaxTemperature() == tempMax && getMinTemperature() == tempMin;
    }

    bool setPhAlarm(int phMin, int phMax)
    {
        if (phMax < phMin || phMax <= 0 || phMin <= 0)
        {
            Serial.println("Impossivel definir esse ph");
            return false;
        }

        _memory.write<int>(ADDRESS_AQUARIUM_MIN_PH, phMin);
        _memory.write<int>(ADDRESS_AQUARIUM_MAX_PH, phMax);
        
        return getMaxPh() == phMax && getMinPh() == phMin;
    }

    int getMaxPh()
    {
        return _memory.read<int>(ADDRESS_AQUARIUM_MAX_PH);
    }
    int getMinPh()
    {
        return _memory.read<int>(ADDRESS_AQUARIUM_MIN_PH);
    }

    int getMaxTemperature()
    {
        return _memory.read<int>(ADDRESS_AQUARIUM_MAX_TEMPERATURE);
    }
    int getMinTemperature()
    {
        return _memory.read<int>(ADDRESS_AQUARIUM_MIN_TEMPERATURE);
    }

    bool getWaterPumpStatus()
    {
        return digitalRead(PIN_WATER_PUMP);
    }

    void setWaterPumpStatus(bool status)
    {
        digitalWrite(PIN_WATER_PUMP, !status);
    }
    void setStatusHeater(bool status)
    {
        digitalWrite(PIN_HEATER, !status);
    }
    
    void setPeristaulticStatus(double ml, solution solution)
    {
        //double time =  ml / FLUXO_PERISTALTIC_ML_S;
        double time =  ml / (100.0 / 60.0);

        Serial.printf((solution != SOLUTION_LOWER) ? "RAISER" : "LOWER");
        Serial.printf("\r\nML: %lf - Time: %lf\r\n", ml, time);


        if(solution == SOLUTION_LOWER){
            digitalWrite(PIN_PERISTAULTIC_RAISER, false);
            digitalWrite(PIN_PERISTAULTIC_LOWER, true);
            vTaskDelay(time * 1000 / portTICK_PERIOD_MS);
            digitalWrite(PIN_PERISTAULTIC_LOWER, false);
            return;
        }
        if(solution == SOLUTION_RAISER){
            digitalWrite(PIN_PERISTAULTIC_LOWER, false);
            digitalWrite(PIN_PERISTAULTIC_RAISER, true);
            vTaskDelay(time * 1000 / portTICK_PERIOD_MS);
            digitalWrite(PIN_PERISTAULTIC_RAISER, false);
            analogWrite(PIN_PERISTAULTIC_RAISER , 0);
            return;
        }
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }

    void handlerWaterPump(Date now) {
        try
        {
            vector<routine> rotinas = readRoutine();
            for (const auto& routine : rotinas) {
                if(routine.weekday[now.day_of_week]){
                    for (const auto& h : routine.horarios) {
                        if((now.hour * 60 + now.minute) >= h.start  && (now.hour * 60 + now.minute) < h.end){
                            setWaterPumpStatus(HIGH);
                            return;
                        }
                        setWaterPumpStatus(LOW);
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

    Date getEndNextTime(){

        return Date();
    }

    bool setPPM(int dosagem)
    {
        if (dosagem <= 0)
        {
            Serial.println("Impossivel definir esse dosagem");
            return false;
        }

        _memory.write<int>(ADDRESS_PPM_PH, dosagem);
        
        return getPPM() == dosagem;
    }
    int getPPM()
    {
        return _memory.read<int>(ADDRESS_PPM_PH);
    }

    int getTurbidity()
    {
        double voltagem = analogRead(PIN_TURBIDITY) / 4095.0 * 3.3;

        if (voltagem <= 2.5)
            return 3000;

        if (voltagem > 4.2)
            return 0;
            
        return -1120.4 * sqrt(voltagem) + 5742.3 * voltagem - 4353.8;
    }
};
#endif