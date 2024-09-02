#ifndef BLUETOOTH_CALLBACK
#define BLUETOOTH_CALLBACK

#include <AsyncJson.h>
#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "base64.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>


class BluetoothCallback : public BLECharacteristicCallbacks 
{
private:
    std::string request = "";
    std::string responseData = "";
    size_t expectedLength = 0;
    bool isReceivingMessage = false;


    void onWrite(BLECharacteristic* characteristic) {
        Serial.printf("WRITE:\n");

        std::string value = characteristic->getValue();

        if(static_cast<unsigned char>(value.back()) != 0xFF){
            if(!isReceivingMessage){
                isReceivingMessage = true;
                
                request.clear();
            }
            request += value;
            return;
        }
        
        isReceivingMessage = false;
        Serial.println("end");
        
        if(!onWriteCallback) return;

        try {
            DynamicJsonDocument docRequest = tryDesserialize(!request.empty() ? request.c_str() : "{}");
            Serial.printf("[LOG] [BLE:WRITE]: %s\r\n", request.c_str());

            DynamicJsonDocument response = onWriteCallback(&docRequest);
            docRequest.clear();
            
            responseData = trySerialize(&response);
        }
        catch (const std::exception& e)
        {
            uint8_t endSignal = 0xFF;
            characteristic->setValue(&endSignal, sizeof(endSignal));
            Serial.printf("[write] erro: %s\n", e.what());
        }
    }

    void onRead(BLECharacteristic* characteristic) {
        Serial.printf("READ:\n");

        if(!onReadCallback || isReceivingMessage){
            return;
        }

        try {
            DynamicJsonDocument oldValue = tryDesserialize("{}");
            DynamicJsonDocument respDoc = onReadCallback(&oldValue);
            oldValue.clear();

            sendBLE(characteristic, trySerialize(&respDoc));
        }
        catch (const std::exception& e)
        {
            uint8_t endSignal = 0xFF;
            characteristic->setValue(&endSignal, sizeof(endSignal));
            Serial.printf("[read] erro: %s\n", e.what());
        }
    }
    void onNotify(BLECharacteristic* characteristic) {
        Serial.printf("NOTIFY\n");
        
        if(isReceivingMessage){
            return;
        }
        // Serial.printf("data: %s\r\n", characteristic->getValue().c_str());
    }

    void sendBLE(BLECharacteristic* characteristic, const char* value){
        size_t dataLength = strlen(value);
        std::string valueStr(value);
        
        if (dataLength <= 0) { return; }

        size_t offset = 0;
        std::vector<std::string> chunksList;

        while (offset < dataLength) {
            size_t chunkSize = std::min(MAX_SIZE, dataLength - offset);
            chunksList.push_back(valueStr.substr(offset, chunkSize));
            offset += chunkSize;
        }

        for (size_t i = 0; i < chunksList.size(); i++) {
            characteristic->setValue(chunksList[i].c_str());
            characteristic->notify();
            Serial.printf("set: %s\r\n", chunksList[i].c_str());
        }
        valueStr.clear();
        
        uint8_t endSignal = 0xFF;
        characteristic->setValue(&endSignal, sizeof(endSignal));
        characteristic->notify();
    }
 
    DynamicJsonDocument tryDesserialize(const char*  data){
        DynamicJsonDocument doc(35000);

        DeserializationError error = deserializeJson(doc, data);
        
        if (error) {
        //     throw std::runtime_error(("Falha na desserialização: %s" + String(error.f_str())).c_str());
            throw std::runtime_error("Falha na desserialização");
        }

        return doc;
    }
    const char* trySerialize(DynamicJsonDocument *doc){
        std::string data;
        serializeJson((*doc), data);
        
        return data.c_str();
    }
public:
    BluetoothCallback();
    ~BluetoothCallback();

    std::function<DynamicJsonDocument(DynamicJsonDocument*)> onReadCallback; 
    std::function<DynamicJsonDocument(DynamicJsonDocument*)> onWriteCallback;

    void notify(BLECharacteristic* characteristic, const char* value);
};

BluetoothCallback::BluetoothCallback() 
{
}

BluetoothCallback::~BluetoothCallback()
{
}

void BluetoothCallback::notify(BLECharacteristic* characteristic, const char* value)
{
    sendBLE(characteristic, value);
}
#endif