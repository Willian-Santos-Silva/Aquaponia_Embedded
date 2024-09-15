
#ifndef SETUP_DEVICE_H
#define SETUP_DEVICE_H

#include "Aquarium.h"
#include "Routines.h"
#include "Aplicacoes.h"

#include "Base/memory.h"

#include "FS.h"
#include "SPIFFS.h"

#include "uuid.h"


class SetupDevice {
private:
    Memory* _memory;
    Aquarium *_aquarium;

    void close(File file){
        file.close();
        SPIFFS.end();
    }

    File open(const char* fileFullPath, const char *mode = "r"){
        if (!SPIFFS.begin(true)) {
            throw std::runtime_error("Falha ao montar o sistema de arquivos SPIFFS");
        }
        
        File file = SPIFFS.open(fileFullPath, mode);
        if (!file) {
            Serial.println("Arquivo nao existe");
            
            SPIFFS.end();
            throw std::runtime_error("Erro ao abrir o arquivo para leitura");
        }
        return file;
    }

    void removeIfExists (const char* fileFullPath) {
        if (!SPIFFS.begin(true)) {
            throw std::runtime_error("Falha ao montar o sistema de arquivos SPIFFS");
        }
        if(SPIFFS.exists("/pump.bin")){
            SPIFFS.remove("/pump.bin");
            SPIFFS.end();
        }
    }

public:
    SetupDevice(Aquarium *aquarium, Memory* memory) : _aquarium(aquarium), _memory(memory) {

    }
    
    void begin(){
        if (_memory->readBool(ADDRESS_START)){
            Serial.println("Startado");
            return;
        }

        Serial.printf(_aquarium->setHeaterAlarm(MIN_AQUARIUM_TEMP, MAX_AQUARIUM_TEMP) ? "TEMPERATURAS DEFINIDAS" : "FALHA AO DEFINIR INTERVALO DE TEMPERATURA");
        Serial.printf("\n");
        Serial.printf(_aquarium->setPhAlarm(MIN_AQUARIUM_PH, MAX_AQUARIUM_PH) ? "PH DEFINIDO" : "FALHA AO DEFINIR INTERVALO DE PH");


        if (!SPIFFS.begin(true)) {
            throw std::runtime_error("Falha ao montar o sistema de arquivos SPIFFS");
        }

        if(SPIFFS.exists("/rotinas.bin"))
            SPIFFS.remove("/rotinas.bin");

        if(SPIFFS.exists("/pump.bin"))
            SPIFFS.remove("/pump.bin");

        SPIFFS.end();

        _memory->writeBool(ADDRESS_START, true);
    }


    vector<aplicacoes> readAplicacao()
    {
        vector<aplicacoes> aplicacoesList = {};

        File file = open("/rotinas.bin", FILE_READ);

        uint32_t aplicacoesSize;
        if (file.read((uint8_t *)&aplicacoesSize, sizeof(aplicacoesSize)) != sizeof(aplicacoesSize)){
            Serial.println("Erro ao ler as aplicacoes");
            
            close(file);
            
            return {};
        }
        
        for (uint32_t i = 0; i < aplicacoesSize; ++i) {
            aplicacoes aplicacao;

            if (file.read((uint8_t *)&aplicacao.dataAplicacao, sizeof(aplicacao.dataAplicacao)) != sizeof(aplicacao.dataAplicacao)){
                Serial.println("Erro ao ler o dataAplicacao");
                
                close(file);
                return {};
            }

            if (file.read((uint8_t *)&aplicacao.ml, sizeof(aplicacao.ml)) != sizeof(aplicacao.ml)){
                Serial.println("Erro ao ler o ml");
                
                close(file);
                return {};
            }
            if (file.read((uint8_t *)&aplicacao.type, sizeof(aplicacao.type)) != sizeof(aplicacao.type)){
                Serial.println("Erro ao ler o type");
                
                close(file);
                return {};
            }
            if (file.read((uint8_t *)&aplicacao.deltaPh, sizeof(aplicacao.deltaPh)) != sizeof(aplicacao.deltaPh)){
                Serial.println("Erro ao ler o type");
                
                close(file);
                return {};
            }
            aplicacoesList.push_back(aplicacao);
        }
        
        close(file);
        
        return aplicacoesList;
    }

    void addAplicacao(const vector<aplicacoes> &aplicacoesList)
    {
        removeIfExists("/pump.bin");
        File file = open("/pump.bin", FILE_WRITE);

        uint32_t aplicacoesSize = aplicacoesList.size();
        file.write((uint8_t *)&aplicacoesSize, sizeof(aplicacoesSize));

        for (const auto &aplicacao : aplicacoesList){
            file.write((uint8_t*)&aplicacao.dataAplicacao, sizeof(aplicacao.dataAplicacao));
            file.write((uint8_t*)&aplicacao.ml, sizeof(aplicacao.ml));
            file.write((uint8_t*)&aplicacao.type, sizeof(aplicacao.type));
            file.write((uint8_t*)&aplicacao.deltaPh, sizeof(aplicacao.deltaPh));
        }

        close(file);
    }


    void writeRoutine(const vector<routine> &routines)
    {
        removeIfExists("/rotinas.bin");
        File file = SPIFFS.open("/rotinas.bin", FILE_WRITE);
        
        uint32_t routinesSize = routines.size();
        file.write((uint8_t *)&routinesSize, sizeof(routinesSize));

        for (const auto &r : routines) {
            file.write((uint8_t*)r.id, sizeof(r.id));
            file.write((uint8_t *)r.weekday, sizeof(r.weekday));

            uint32_t horariosSize = r.horarios.size();
            file.write((uint8_t *)&horariosSize, sizeof(horariosSize));

            for (const auto &h : r.horarios) {
                file.write((uint8_t *)&h, sizeof(h));
            }
        }

        close(file);
    }

    vector<routine> readRoutine()
    {
        vector<routine> routines = {};

        File file = open("/rotinas.bin", FILE_READ);

        uint32_t routinesSize;
        if (file.read((uint8_t *)&routinesSize, sizeof(routinesSize)) != sizeof(routinesSize)){
            Serial.println("Erro ao ler as rotinas");
            close(file);
            return {};
        }
        
        for (uint32_t i = 0; i < routinesSize; ++i) {
            routine r;
            if (file.read((uint8_t*)r.id, sizeof(r.id)) != sizeof(r.id)) {
                Serial.println("Erro ao ler o ID da rotina");
                close(file);
                return {};
            }

            if (file.read((uint8_t *)&r.weekday, sizeof(r.weekday)) != sizeof(r.weekday)){
                Serial.println("Erro ao ler o weekday da rotina");
                close(file);
                return {};
            }

            uint32_t horariosSize;
            if (file.read((uint8_t *)&horariosSize, sizeof(horariosSize)) != sizeof(horariosSize)){
                Serial.println("Erro ao ler o numero de horarios da rotina");
                close(file);
                return {};
            }

            for (uint32_t j = 0; j < horariosSize; ++j) {
                horario h;
                if(file.read((uint8_t *)&h, sizeof(h)) != sizeof(h)){
                    Serial.println("Erro ao ler o horario da rotina");
                    close(file);
                    return {};
                }
                r.horarios.push_back(h);
            }

            routines.push_back(r);
        }
        
        close(file);

        return routines;
    }
};

#endif