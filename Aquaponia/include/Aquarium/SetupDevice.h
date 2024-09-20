
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
        if (_memory.readBool(ADDRESS_START)){
            Serial.println("===================");
            Serial.println("STARTADO");
            Serial.println("===================");
            return;
        }
        Clock clockUTC;
        time_t timestamp = 1726589003;
        tm * time = gmtime(&timestamp);
        clockUTC.setRTC(time);

        Serial.printf(_aquarium->setHeaterAlarm(MIN_AQUARIUM_TEMP, MAX_AQUARIUM_TEMP) ? "TEMPERATURAS DEFINIDAS" : "FALHA AO DEFINIR INTERVALO DE TEMPERATURA");
        Serial.printf("\r\n");
        Serial.printf(_aquarium->setPhAlarm(MIN_AQUARIUM_PH, MAX_AQUARIUM_PH) ? "PH DEFINIDO" : "FALHA AO DEFINIR INTERVALO DE PH");
        Serial.printf("\r\n");
        Serial.printf(_aquarium->setPPM(1) ? "PPM DEFINIDO" : "FALHA AO DEFINIR PPM");
        Serial.printf("\r\n\r\n");

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
            Serial.printf("Erro ao ler arquivo: %s\r\n\r\n", fileFullPath);
            
            close(&file);
            
            return {};
        }

        Serial.printf("Tamanho: %i\r\n\r\n", size);

        vector<T> list(size);
        
        file.read(reinterpret_cast<uint8_t*>(&list[0]), size * sizeof(T));
        
        close(&file);
        
        return list;
    }



    void write(JsonDocument * doc, const char* fileFullPath)
    {
        removeIfExists(fileFullPath);

        File file = open(fileFullPath, FILE_WRITE);
        
        // uint8_t buffer[128];
        // size_t n = serializeMsgPack((*doc), buffer, sizeof(buffer));
        serializeMsgPack((*doc), file);
        
        close(&file);
    }

    JsonDocument  readJSON(const char* fileFullPath)
    {
        File file = open(fileFullPath, FILE_READ);
        
        Serial.printf("%i", file.size());
        JsonDocument doc;

        // DeserializationError error = deserializeMsgPack(doc, file);

        // if (error) {
        //     throw std::runtime_error("Falha na desserialização");
        // }
        // string s;
        // deserializeMsgPack(doc, s);
        
        // size_t size = file.size();
        // uint8_t buffer[size];
        // file.read(buffer, size);
        
        // while(file.available()){
        //     // Serial.write(file.read());
        // }
            // Serial.println(buffer);
        DeserializationError error = deserializeMsgPack(doc, file);

        return doc;
    }

};

#endif