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
    Memory _memory;

    double _temperature;
    SemaphoreHandle_t semaphoreTemperature;

public:
    Aquarium() : ds(PIN_THERMOCOUPLE)
    {
        semaphoreTemperature = xSemaphoreCreateBinary();
        xSemaphoreGive(semaphoreTemperature);
        // ds.selectNext();
    }
    enum solution { SOLUTION_LOWER, SOLUTION_RAISER };

    void begin()
    {
        pinMode(PIN_RESET, INPUT_PULLDOWN);
        pinMode(PIN_HEATER, OUTPUT);
        pinMode(PIN_WATER_PUMP, OUTPUT);

        setStatusHeater(LOW);
        setWaterPumpStatus(LOW);

        pinMode(PIN_PH, INPUT);
    
        pinMode(PIN_PERISTAULTIC_RAISER, OUTPUT);
        pinMode(PIN_PERISTAULTIC_LOWER, OUTPUT);

        ledcAttachPin(PIN_PERISTAULTIC_RAISER, SOLUTION_RAISER);
        ledcSetup(SOLUTION_RAISER, 1000, 10);

        ledcAttachPin(PIN_PERISTAULTIC_LOWER, SOLUTION_LOWER);
        ledcSetup(SOLUTION_LOWER, 1000, 10);

        ledcWrite(SOLUTION_LOWER, calcPotencia(0));
        ledcWrite(SOLUTION_RAISER, calcPotencia(0));
    }

    double updateTemperature()
    {
        if(xSemaphoreTake(semaphoreTemperature, portMAX_DELAY) == pdTRUE) {
            if (ds.getNumberOfDevices() == 0){
                _temperature = -127.0f;
                xSemaphoreGive(semaphoreTemperature);
                return _temperature;
            }
            // uint8_t *add;
            // add[0] = ds.selectNext();

            _temperature = ds.getTempC();
            xSemaphoreGive(semaphoreTemperature);
        }
        
        return _temperature;
    }

    double readTemperature()
    {
        // semaphoreTemperature = xSemaphoreCreateBinary();
        // if(semaphoreTemperature == NULL)
        // return
        if(xSemaphoreTake(semaphoreTemperature, portMAX_DELAY) == pdTRUE) {
            if (ds.getNumberOfDevices() == 0){
                _temperature = -127.0f;
                xSemaphoreGive(semaphoreTemperature);
                return _temperature;
            }
            // uint8_t *add;
            ds.selectNext();

            _temperature = ds.getTempC();
            xSemaphoreGive(semaphoreTemperature);
        }
        
        return _temperature;
    }

    double getPh()
    {
        unsigned long int avgValue; 
        int buf[10],temp;
        
        double Vmax = 3.3;
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

        double pHVol=(double)avgValue*Vmax/Dmax/6;
        double phValue = -3.30 * pHVol + 21.34;

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
        double time =  ml / (FLUXO_PERISTALTIC_ML_S * (POTENCIA_PERISTAULTIC / 100.0));
        
        if(solution == SOLUTION_LOWER) {
            ledcWrite(SOLUTION_RAISER, calcPotencia(0));
            ledcWrite(SOLUTION_LOWER, calcPotencia(POTENCIA_PERISTAULTIC));

            vTaskDelay(pdMS_TO_TICKS(time * 1000.0));
            
            ledcWrite(SOLUTION_LOWER, calcPotencia(0));

            return;
        }

        if(solution == SOLUTION_RAISER){
            ledcWrite(SOLUTION_LOWER, calcPotencia(0));
            ledcWrite(SOLUTION_RAISER, calcPotencia(POTENCIA_PERISTAULTIC));

            vTaskDelay(pdMS_TO_TICKS(time * 1000.0));

            ledcWrite(SOLUTION_RAISER, calcPotencia(0));
            return;
        }
    }
    
    bool setLowerSolutionDosage(int dosagem){
        if (dosagem <= 0)
        {
            return false;
        }

        _memory.write<int>(ADDRESS_DOSAGEM_REDUTOR_PH, dosagem);
        
        return getLowerSolutionDosage() == dosagem;
    }

    int getLowerSolutionDosage()
    {
        return _memory.read<int>(ADDRESS_DOSAGEM_REDUTOR_PH);
    }
    
    bool setRaiserSolutionDosage(int dosagem){
        if (dosagem <= 0)
        {
            return false;
        }

        _memory.write<int>(ADDRESS_DOSAGEM_AUMENTADOR_PH, dosagem);
        
        return getRaiserSolutionDosage() == dosagem;
    }

    int getRaiserSolutionDosage()
    {
        return _memory.read<int>(ADDRESS_DOSAGEM_AUMENTADOR_PH);
    }

    bool setTempoReaplicacao(long tempo_reaplicacao) {
        _memory.write<int>(ADDRESS_CYCLE_TIME_DOSAGEM, tempo_reaplicacao);
        
        return getTempoReaplicacao() == tempo_reaplicacao;
    }

    long getTempoReaplicacao()
    {
        return _memory.read<long>(ADDRESS_CYCLE_TIME_DOSAGEM);
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

    int calcPotencia(double percentage){
        return 1023 * (percentage / 100.0);
    }
    
};
#endif
