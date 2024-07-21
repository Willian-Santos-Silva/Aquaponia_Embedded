#ifndef AQUARIUM_H
#define AQUARIUM_H

#include <DS18B20.h>

#include "config.h"
#include "memory.h"

#define PH4502C_TEMPERATURE_PIN 35
#define PH4502C_PH_PIN 34
#define PH4502C_PH_TRIGGER_PIN 14 
#define PH4502C_CALIBRATION 14.8f
#define PH4502C_READING_INTERVAL 100
#define PH4502C_READING_COUNT 10
#define ADC_RESOLUTION 4096.0f

class Aquarium
{
private:
    DS18B20 ds;
    Memory _memory;

    int sensorValue = 0; 
    unsigned long int avgValue; 
    float b;
    int buf[10],temp;

public:
    enum solution {
      SOLUTION_LOWER,
      SOLUTION_RAISER
    };
    vector<routine> rotinas;

    Aquarium() : ds(PIN_THERMOCOUPLE)
    {
    }

    void begin()
    {
        pinMode(PIN_HEATER, OUTPUT);
        pinMode(PIN_WATER_PUMP, OUTPUT);
        pinMode(PIN_PH, INPUT);
        pinMode(PIN_TURBIDITY, INPUT);

        Serial.print("Inicial: ");
        Serial.println(_memory.read<int>(ADDRESS_AQUARIUM_MIN_TEMPERATURE));
        Serial.print("Inicial: ");
        Serial.println(_memory.read<int>(ADDRESS_AQUARIUM_MIN_PH));
        Serial.println(_memory.read<bool>(ADDRESS_START));
        if (_memory.read<bool>(ADDRESS_START))
            return;

        Serial.println("Eu tentei");

        Serial.print(setHeaterAlarm(MIN_AQUARIUM_TEMP, MAX_AQUARIUM_TEMP) ? "Eu consegui papis: " : "Falhei papito: ");

        Serial.print(getMinTemperature());
        Serial.print(" - ");
        Serial.println(getMaxTemperature());
        
        Serial.print(setPhAlarm(MIN_AQUARIUM_PH, MAX_AQUARIUM_PH) ? "Eu consegui papis: " : "Falhei papito: ");
        Serial.print(getMinPh());
        Serial.print(" - ");
        Serial.println(getMaxPh());
    }
    float map(float x, long in_min, long in_max, float out_min, float out_max) {
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }
    float readTemperature()
    {
        if (ds.getNumberOfDevices() == 0)
            return -127.0f;

        ds.selectNext();
        float temperature = ds.getTempC();
        return temperature;
    }

    float getPh()
    {
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
    
    void setPeristaulticStatus(int ml, solution solution)
    {
        int time =  ml / FLUXO_PERISTALTIC_ML_S;

        if(solution == SOLUTION_LOWER){
            digitalWrite(PIN_PERISTAULTIC, true);
            delay(time * 1000);
            digitalWrite(PIN_PERISTAULTIC, false);
            return;
        }
        if(solution == SOLUTION_RAISER){
            digitalWrite(PIN_PERISTAULTIC, true);
            delay(time * 1000);
            digitalWrite(PIN_PERISTAULTIC, false);
            return;
        }
    }

    void handlerWaterPump(Date now) {
        try
        {
            _memory.loadDataFromEEPROM(rotinas);
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
        return analogRead(PIN_TURBIDITY);
    }
};

#endif