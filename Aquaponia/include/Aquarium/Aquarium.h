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
            _temperature = ds.getTempC();
            xSemaphoreGive(semaphoreTemperature);
        }
        
        return _temperature;
    }

    double readTemperature()
    {
        if(xSemaphoreTake(semaphoreTemperature, portMAX_DELAY) == pdTRUE) {
            if (ds.getNumberOfDevices() == 0){
                _temperature = -127.0f;
                xSemaphoreGive(semaphoreTemperature);
                return _temperature;
            }
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
        double phValue = -5.70 * pHVol + 21.34;

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
        return std::min(_memory.read<int>(ADDRESS_DOSAGEM_REDUTOR_PH), DOSAGE_LOWER_SOLUTION_ML_L);
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
        return std::min(_memory.read<int>(ADDRESS_DOSAGEM_AUMENTADOR_PH), DOSAGE_RAISE_SOLUTION_ML_L);
    }

    bool setTempoReaplicacao(long tempo_reaplicacao) {
        _memory.write<int>(ADDRESS_CYCLE_TIME_DOSAGEM, tempo_reaplicacao);
        
        return getTempoReaplicacao() == tempo_reaplicacao;
    }

    long getTempoReaplicacao()
    {
        return _memory.read<long>(ADDRESS_CYCLE_TIME_DOSAGEM);
    }


    double getTurbidity()
    {
        double tensao = analogRead(PIN_TURBIDITY) / 4095.0 * 3.3;
        double tensao_dp = round_to_dp(tensao, 2);
        // Serial.printf("TURBIDADE SINAL: %lu\r\n", analogRead(PIN_TURBIDITY));
        // Serial.printf("TURBIDADE TENSAO: %lf\r\n", tensao);
        Serial.printf("TURBIDADE ntu 1: %lf\r\n", (-1120.4 * sqrt(tensao_dp) + 5742.3 * tensao_dp - 4353.8));
        Serial.printf("TURBIDADE ntu 2: %lf\r\n", (-19.709 * sqrt(tensao) - 381.66 + 1155.325));

        // if (tensao <= 2.5)
        //     return 3000;

        // if (tensao > 4.2)
        //     return 0;

        // return -1120.4 * sqrt(tensao_dp) + 5742.3 * tensao_dp - 4353.8;
            
        return -19.709 * sqrt(tensao) - 381.66 + 1155.325;
    }

    float round_to_dp(double in_value, int decimal_place){
        float multiplier = powf(10.0f, decimal_place);
        return roundf(in_value * multiplier) / multiplier;
    }

    int calcPotencia(double percentage){
        return 1023 * (percentage / 100.0);
    }
    
};
#endif
