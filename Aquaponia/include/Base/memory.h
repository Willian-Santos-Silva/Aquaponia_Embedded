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
            throw std::runtime_error("Falha ao inicializar eeprom");
        }
        
        for (int i = 0; i < sizeof(T); i++)
        {
            EEPROM.put(address, data  >> (8 * i));
            address++;
        }
        
        EEPROM.commit();
    }

    void writeLong(int address, unsigned long data)
    {
        if (!EEPROM.begin(EEPROM_SIZE))
        {
            throw std::runtime_error("Falha ao inicializar eeprom");
        }
        
        EEPROM.writeULong(address, data);
        
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

    void writeBool(int address, bool data)
    {
        if (!EEPROM.begin(EEPROM_SIZE))
        {
            throw std::runtime_error("Falha ao inicializar eeprom");
        }
        
        EEPROM.writeBool(address, data);
        
        EEPROM.commit();
    }
    bool readBool(int address)
    {
        if (!EEPROM.begin(EEPROM_SIZE))
        {
            throw std::runtime_error("Falha ao inicializar eeprom");
        }
        return EEPROM.readBool(address);
    }


    void clear(){
        Serial.println("Limpando...");

        if (!EEPROM.begin(EEPROM_SIZE))
        {
            throw std::runtime_error("Falha ao inicializar eeprom");
        }

        for (int i = 0 ; i < EEPROM_SIZE; i++) {
            EEPROM.write(i, 0x00);
        }
        EEPROM.commit();
        EEPROM.end();
        Serial.println("Limpeza concluida");
    }
};

#endif