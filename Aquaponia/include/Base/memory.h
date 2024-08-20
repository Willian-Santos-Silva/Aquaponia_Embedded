#ifndef MEMORY_H
#define MEMORY_H

#include <EEPROM.h>
#include <vector>

#include "Base/config.h"

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
        
        // if constexpr (std::is_same<T, const char*>::value)
        // {
        //     int i = 0;
        //     while (data[i] != '\0') {
        //         EEPROM.write(address, data[i]);
        //         address++;
        //         i++;
        //     }
        // }
        // else
        // {
        //     for (int i = 0; i < sizeof(T); i++)
        //     {
        //         EEPROM.write(address, (data >> (8 * i)) & 0xFF);
        //         address++;
        //     }
        // }

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

    void clear(){
        Serial.println("Limpando...");
        for (int i = 0 ; i < EEPROM.length() ; i++) {
            EEPROM.write(i, 0);
        }
        EEPROM.commit();
        Serial.println("Limpeza concluida");
    }
};

#endif