#ifndef AQUARIUM_H
#define AQUARIUM_H

#include <DS18B20.h>

#include "config.h"
#include "memory.h"

class Aquarium
{
private:
    DS18B20 ds;
    Memory _memory;

public:
    Aquarium() : ds(PIN_THERMOCOUPLE)
    {
    }

    void begin()
    {
        pinMode(PIN_HEATER, OUTPUT);
        pinMode(PIN_WATER_PUMP, OUTPUT);

        Serial.print("Valor Inicial: ");
        Serial.println(_memory.read<int>(ADDRESS_AQUARIUM_MAX_TEMPERATURE));

        if (_memory.read<int>(ADDRESS_AQUARIUM_MIN_TEMPERATURE) == 0 || _memory.read<int>(ADDRESS_AQUARIUM_MAX_TEMPERATURE) == 0)
        {
            setHeaterAlarm(MIN_AQUARIUM_TEMP, MAX_AQUARIUM_TEMP);
        }
    }

    float readTemperature()
    {
        if (ds.getNumberOfDevices() == 0)
            return -127.0f;

        ds.selectNext();
        float temperature = ds.getTempC();
        return temperature;
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

    bool getCoolingStatus()
    {
        return digitalRead(PIN_COOLING);
    }
    void setStatusHeater(bool status)
    {
        digitalWrite(PIN_HEATER, status);
    }
    void setStatusCooling(bool status)
    {
        digitalWrite(PIN_COOLING, status);
    }
};

#endif