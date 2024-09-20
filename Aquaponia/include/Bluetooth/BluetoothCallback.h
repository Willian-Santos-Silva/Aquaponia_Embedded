#ifndef BLUETOOTH_CALLBACK
#define BLUETOOTH_CALLBACK

// #include <AsyncJson.h>
#include <ArduinoJson.h>
#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "base64.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>


SemaphoreHandle_t isNotify = xSemaphoreCreateBinary();

class BluetoothCallback : public BLECharacteristicCallbacks 
{
private:
    std::string request = "";
    std::string responseData = "";
    size_t expectedLength = 0;
    bool isReceivingMessage = false;


    void onWrite(BLECharacteristic* characteristic) {
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
        
        if(!onWriteCallback) return;

        try {
            JsonDocument  docRequest = tryDesserialize(!request.empty() ? request.c_str() : "{}");
            Serial.printf("[LOG] [BLE:WRITE]: %s\r\n", request.c_str());

            JsonDocument  response = onWriteCallback(&docRequest);
            docRequest.clear();
            
            responseData = trySerialize(&response);
            response.clear();
        }
        catch (const std::exception& e)
        {
            uint8_t endSignal = 0xFF;
            characteristic->setValue(&endSignal, sizeof(endSignal));
            Serial.printf("[write] erro: %s\n", e.what());
        }
    }

    void onRead(BLECharacteristic* characteristic) {
        if(!onReadCallback || isReceivingMessage){
            return;
        }

        try {
            JsonDocument  oldValue = tryDesserialize("{}");
            JsonDocument  respDoc = onReadCallback(&oldValue);
            oldValue.clear();

            notify(characteristic, trySerialize(&respDoc));
            respDoc.clear();
        }
        catch (const std::exception& e)
        {
            uint8_t endSignal = 0xFF;
            characteristic->setValue(&endSignal, sizeof(endSignal));
            Serial.printf("[read] erro: %s\r\n", e.what());
        }
    }
    void onNotify(BLECharacteristic* characteristic) {
        if(isReceivingMessage){
            return;
        }
        // Serial.printf("data: %s\r\n", characteristic->getValue().c_str());
    }

    void sendBLE(BLECharacteristic* characteristic, const std::string& value){
        size_t offset = 0;  
        std::vector<uint8_t> vec(value.begin(), value.end());
        size_t dataLength = vec.size();


        while (offset < dataLength) {
            size_t chunkSize = std::min(MAX_SIZE, dataLength - offset);

            std::string concatenated(vec.begin() + offset, vec.begin() + offset + chunkSize);

            Serial.printf("set: %s\r\n", concatenated.c_str());
            characteristic->setValue(concatenated);
            characteristic->notify();

            offset += chunkSize;
        }
        
        uint8_t endSignal = 0xFF;
        characteristic->setValue(&endSignal, sizeof(endSignal));
        characteristic->notify();
    }
 
    JsonDocument  tryDesserialize(const char*  data){
        JsonDocument doc;

        DeserializationError error = deserializeJson(doc, data);
        
        if (error) {
        //     throw std::runtime_error(("Falha na desserialização: %s" + String(error.f_str())).c_str());
            throw std::runtime_error("Falha na desserialização");
        }

        return doc;
    }
    const char* trySerialize(JsonDocument  *doc){
        std::string data;
        serializeJson((*doc), data);
        
        return data.c_str();
    }
public:
    BluetoothCallback();
    ~BluetoothCallback();

    std::function<JsonDocument (JsonDocument *)> onReadCallback; 
    std::function<JsonDocument (JsonDocument *)> onWriteCallback;

    void notify(BLECharacteristic* characteristic, const char* value);
};

BluetoothCallback::BluetoothCallback() 
{xSemaphoreGive(isNotify);
}

BluetoothCallback::~BluetoothCallback()
{
}

void BluetoothCallback::notify(BLECharacteristic* characteristic, const char* value)
{
     if (xSemaphoreTake(isNotify, portMAX_DELAY)) {
        sendBLE(characteristic, value);
        xSemaphoreGive(isNotify);
    }
}
#endif