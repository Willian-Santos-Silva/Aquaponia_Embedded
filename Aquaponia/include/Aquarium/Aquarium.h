#ifndef AQUARIUM_H
#define AQUARIUM_H

#include <OneWire.h>
#include <DS18B20.h>
#include <vector>

#include "Aquarium/Routines.h"
#include "Aquarium/Aplicacoes.h"

#include "Clock/Clock.h"
#include "Clock/Date.h"

#include "Base/config.h"
#include "Base/memory.h"


using namespace std;

class Aquarium
{
private:
    DS18B20 ds;
    Memory* _memory;

    float _temperature; 

public:
    Aquarium(Memory *memory) : _memory(memory), ds(PIN_THERMOCOUPLE)
    {
    }
    enum solution { SOLUTION_LOWER, SOLUTION_RAISER };

    void begin()
    {
        pinMode(PIN_HEATER, OUTPUT);
        pinMode(PIN_WATER_PUMP, OUTPUT);
        pinMode(PIN_PH, INPUT);
        pinMode(PIN_PERISTAULTIC_RAISER, OUTPUT);
        pinMode(PIN_PERISTAULTIC_LOWER, OUTPUT);
    
        pinMode(PIN_PERISTAULTIC_RAISER, OUTPUT);
        pinMode(PIN_PERISTAULTIC_LOWER, OUTPUT);

        ledcAttachPin(PIN_PERISTAULTIC_RAISER, 0);
        ledcSetup(0, 1000, 10);

        ledcAttachPin(PIN_PERISTAULTIC_LOWER, 1);
        ledcSetup(1, 1000, 10);

        ledcWrite(0, potencia(0));
        ledcWrite(1, potencia(0));
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
        
        float Vmax = 5;
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

    bool getHeaterStatus()
    {
        return !digitalRead(PIN_HEATER);
    }

    bool setHeaterAlarm(int tempMin, int tempMax)
    {
        if (tempMax < tempMin || tempMax < -273 || tempMin < -273)
        {
            Serial.println("Impossivel definir essa temperatura");
            return false;
        }

        _memory->write<int>(ADDRESS_AQUARIUM_MIN_TEMPERATURE, tempMin);
        _memory->write<int>(ADDRESS_AQUARIUM_MAX_TEMPERATURE, tempMax);
        
        return getMaxTemperature() == tempMax && getMinTemperature() == tempMin;
    }

    bool setPhAlarm(int phMin, int phMax)
    {
        if (phMax < phMin || phMax <= 0 || phMin <= 0)
        {
            Serial.println("Impossivel definir esse ph");
            return false;
        }

        _memory->write<int>(ADDRESS_AQUARIUM_MIN_PH, phMin);
        _memory->write<int>(ADDRESS_AQUARIUM_MAX_PH, phMax);
        
        return getMaxPh() == phMax && getMinPh() == phMin;
    }

    int getMaxPh()
    {
        return _memory->read<int>(ADDRESS_AQUARIUM_MAX_PH);
    }
    int getMinPh()
    {
        return _memory->read<int>(ADDRESS_AQUARIUM_MIN_PH);
    }

    int getMaxTemperature()
    {
        return _memory->read<int>(ADDRESS_AQUARIUM_MAX_TEMPERATURE);
    }
    int getMinTemperature()
    {
        return _memory->read<int>(ADDRESS_AQUARIUM_MIN_TEMPERATURE);
    }

    bool getWaterPumpStatus()
    {
        return !digitalRead(PIN_WATER_PUMP);
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
        double potencia = 0.2;
        double fluxo_peristaultic_ml_s = 1.25;
        double time =  ml / (fluxo_peristaultic_ml_s * potencia);

        Serial.printf((solution != SOLUTION_LOWER) ? "RAISER" : "LOWER");
        Serial.printf("\r\nML: %lf - Time: %lf\r\n", ml, time);


        if(solution == SOLUTION_LOWER){
            Serial.printf("\r\nTime(ms): %lf\r\n", (time * 1000.0));
            digitalWrite(PIN_PERISTAULTIC_RAISER, HIGH);
            digitalWrite(PIN_PERISTAULTIC_LOWER, LOW);

            vTaskDelay((time * 1000.0) / portTICK_PERIOD_MS);
            
            digitalWrite(PIN_PERISTAULTIC_LOWER, HIGH);
            return;
        }
        if(solution == SOLUTION_RAISER){
            Serial.printf("\r\nTime(ms): %lf\r\n", (time * 1000.0));
            digitalWrite(PIN_PERISTAULTIC_LOWER, HIGH);
            digitalWrite(PIN_PERISTAULTIC_RAISER, LOW);
            

            vTaskDelay((time * 1000.0) / portTICK_PERIOD_MS);
            
            digitalWrite(PIN_PERISTAULTIC_RAISER, HIGH);
            return;
        }
    }


    bool setPPM(int dosagem)
    {
        if (dosagem <= 0)
        {
            Serial.println("Impossivel definir esse dosagem");
            return false;
        }

        _memory->write<int>(ADDRESS_PPM_PH, dosagem);
        
        return getPPM() == dosagem;
    }

    int getPPM()
    {
        return _memory->read<int>(ADDRESS_PPM_PH);
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

    int potencia(double percentage){
        return 1023 * (percentage / 100.0);
    }

};
#endif
