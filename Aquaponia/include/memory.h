#ifndef MEMORY_H
#define MEMORY_H

#include <EEPROM.h>

#include "config.h"

class Memory
{
private:
public:
    Memory()
    {
    }

    template <typename T>
    void write(int address, T data)
    {
        if (!EEPROM.begin(EEPROM_SIZE))
        {
            Serial.println("Falha ao inicializar eeprom");
            return;
        }
        
        for (int i = 0; i < sizeof(T); i++)
        {
            EEPROM.put(address, data  >> (8 * i) & 0xFF);
            address++;
        }

        EEPROM.commit();
    }

    template <typename T>
    T read(int address)
    {
        T data;

        for (int i = 0; i < sizeof(T); i++)
        {
            data |= EEPROM.read(address++) << (8 * i);
        }
        return static_cast<T>(data);
    }
};

#endif