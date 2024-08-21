#ifndef MEMORY_H
#define MEMORY_H

#include <EEPROM.h>

// #include <iostream>
// #include <cstring>
// #include <sstream>

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
        // char* buffer;
        // std::ostringstream oss;
        // oss << data;
        // std::string str = oss.str();
        // std::strncpy(buffer, str.c_str(), sizeof(T) - 1);
        
        EEPROM.put(address, data);

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