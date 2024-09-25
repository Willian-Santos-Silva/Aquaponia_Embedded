
#ifndef SETUP_DEVICE_H
#define SETUP_DEVICE_H

#include "Aquarium.h"
#include "Routines.h"
#include "Aplicacoes.h"
#include "Historico.h"

#include "Base/memory.h"
#include "Clock/Clock.h"

#include "ArduinoJson.h"

#include "FS.h"
#include "SPIFFS.h"

#include "uuid.h"


class SetupDevice {
private:
    Memory _memory;
    Aquarium *_aquarium;

    void close(File *file){
        file->close();
        // SPIFFS.end();
    }

    File open(const char* fileFullPath, const char *mode = "r"){
        if (!SPIFFS.begin(true)) {
            throw std::runtime_error("Falha ao montar o sistema de arquivos SPIFFS");
        }
        
        File file = SPIFFS.open(fileFullPath, mode);
        if (!file) {
            Serial.println("Arquivo nao existe");
            
            // SPIFFS.end();
            throw std::runtime_error("Erro ao abrir o arquivo para leitura");
        }
        return file;
    }

    void removeIfExists (const char* fileFullPath) {
        if (!SPIFFS.begin(true)) {
            throw std::runtime_error("Falha ao montar o sistema de arquivos SPIFFS");
        }
        if(SPIFFS.exists(fileFullPath)){
            SPIFFS.remove(fileFullPath);
            // SPIFFS.end();
        }
    }

public:
    SetupDevice(Aquarium *aquarium) : _aquarium(aquarium) {

    }
    
    void begin(){
        // if (_memory.readBool(ADDRESS_START)){
        //     Serial.println("===================");
        //     Serial.println("STARTADO");
        //     Serial.println("===================");
        //     return;
        // }
        Clock clockUTC;
        time_t timestamp = 1726589003;
        tm * time = gmtime(&timestamp);
        clockUTC.setRTC(time);

        log_d("%s", _aquarium->setHeaterAlarm(MIN_AQUARIUM_TEMP, MAX_AQUARIUM_TEMP) ? "TEMPERATURAS DEFINIDAS" : "FALHA AO DEFINIR INTERVALO DE TEMPERATURA");
        log_d("%s", _aquarium->setPhAlarm(MIN_AQUARIUM_PH, MAX_AQUARIUM_PH) ? "PH DEFINIDO" : "FALHA AO DEFINIR INTERVALO DE PH");
        log_d("%s", _aquarium->setLowerSolutionDosage(DOSAGE_LOWER_SOLUTION_ML_L) ? "DOSAGEM DEFINIDA" : "FALHA AO DEFINIR DOSAGEM");
        log_d("%s", _aquarium->setRaiserSolutionDosage(DOSAGE_RAISE_SOLUTION_ML_L) ? "DOSAGEM DEFINIDA" : "FALHA AO DEFINIR DOSAGEM");
        log_d("%s", _aquarium->setTempoReaplicacao(DEFAULT_TIME_DELAY_PH) ? "TEMPO DE REAPLICAÇÃO DEFINIDO" : "FALHA AO DEFINIR TEMPO DE REAPLICAÇÃO");

        removeIfExists("/rotinas.bin");
        removeIfExists("/pump.bin");

        removeIfExists("/histTemp.bin");
        removeIfExists("/histPh.bin");


        _memory.writeBool(ADDRESS_START, true);
        
    }

    template <typename T>
    void write(vector<T>& list, const char* fileFullPath)
    {
        removeIfExists(fileFullPath);

        File file = open(fileFullPath, FILE_WRITE);

        uint32_t size = list.size();
        file.write((uint8_t *)&size, sizeof(size));

        file.write(reinterpret_cast<uint8_t*>(&list[0]), size * sizeof(T));

        close(&file);
    }



    template <typename T>
    vector<T> read(const char* fileFullPath)
    {
        File file = open(fileFullPath, FILE_READ);

        uint32_t size;
        if (file.read((uint8_t *)&size, sizeof(size)) != sizeof(size)){
            log_e("Erro ao ler arquivo: %s", fileFullPath);
            
            close(&file);
            
            return {};
        }
        vector<T> list(size);
        
        file.read(reinterpret_cast<uint8_t*>(&list[0]), size * sizeof(T));
        
        close(&file);
        
        return list;
    }

};

#endif