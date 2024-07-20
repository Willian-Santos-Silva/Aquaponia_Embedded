#ifndef MEMORY_H
#define MEMORY_H

#include <EEPROM.h>
#include <vector>

#include "config.h"

struct horario{
    int start;
    int end;
};
struct routine{
    bool weekday[7];
    vector<horario> horarios;
};
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

    void clear(){
        Serial.println("Limpando...");
        for (int i = 0 ; i < EEPROM.length() ; i++) {
            EEPROM.write(i, 0);
        }
        Serial.println("Limpeza concluida");
    }



    void deserializeRoutine(routine &r, const byte *buffer, int &pos) {
        for (int i = 0; i < 7; ++i) {
            r.weekday[i] = buffer[pos++];
        }
        int horarioSize;
        memcpy(&horarioSize, buffer + pos, sizeof(int));
        pos += sizeof(int);
        r.horarios.resize(horarioSize);
        for (int i = 0; i < horarioSize; ++i) {
            memcpy(&r.horarios[i].start, buffer + pos, sizeof(int));
            pos += sizeof(int);
            memcpy(&r.horarios[i].end, buffer + pos, sizeof(int));
            pos += sizeof(int);
        }
    }

    void serializeRoutine(const routine &r, byte *buffer, int &pos) {
        for (int i = 0; i < 7; ++i) {
            buffer[pos++] = r.weekday[i];
        }
        int horarioSize = r.horarios.size();
        memcpy(buffer + pos, &horarioSize, sizeof(int));
        pos += sizeof(int);
        for (const auto &h : r.horarios) {
            memcpy(buffer + pos, &h.start, sizeof(int));
            pos += sizeof(int);
            memcpy(buffer + pos, &h.end, sizeof(int));
            pos += sizeof(int);
        }
    }

    void saveDataToEEPROM(const std::vector<routine> &data) {
        int dataSize = data.size();
        byte buffer[SIZE_ROUTINES];
        int pos = 0;
        
        memcpy(buffer + pos, &dataSize, sizeof(int));
        pos += sizeof(int);
        
        for (const auto &r : data) {
            serializeRoutine(r, buffer, pos);
        }
        if (!EEPROM.begin(SIZE_ROUTINES)) {
            Serial.println("Failed to initialise EEPROM");
            return;
        }
        for (int i = 0; i < SIZE_ROUTINES; ++i) {
            EEPROM.write(i, buffer[i]);
        }
        EEPROM.commit();
    }

    void loadDataFromEEPROM(std::vector<routine> &data) {
        byte buffer[SIZE_ROUTINES];
        int pos = 0;
        
        for (int i = 0; i < SIZE_ROUTINES; ++i) {
            buffer[i] = EEPROM.read(i);
        }
        
        int dataSize;
        memcpy(&dataSize, buffer + pos, sizeof(int));
        pos += sizeof(int);
        
        data.resize(dataSize);
        for (auto &r : data) {
            deserializeRoutine(r, buffer, pos);
        }
    }
};

#endif