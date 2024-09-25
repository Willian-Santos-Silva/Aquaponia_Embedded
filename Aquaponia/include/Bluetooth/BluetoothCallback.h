#ifndef BLUETOOTH_CALLBACK
#define BLUETOOTH_CALLBACK

// #include <AsyncJson.h>
#include <ArduinoJson.h>
#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "base64.h"

#include <NimBLEDevice.h>


// SemaphoreHandle_t isNotify = xSemaphoreCreateBinary();

class BluetoothCallback : public NimBLECharacteristicCallbacks 
{
private:
    String request = "";
    String responseData = "";
    size_t expectedLength = 0;
    bool isReceivingMessage = false;


    void onWrite(NimBLECharacteristic* characteristic) {
        String value = characteristic->getValue().c_str();

        if(value.charAt(value.length() - 1) != 0xFF){
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
            JsonDocument docRequest = tryDesserialize(!request.isEmpty() ? request.c_str() : "{}");
            Serial.printf("[LOG] [BLE:WRITE]: %s\r\n", request.c_str());

            JsonDocument response = onWriteCallback(&docRequest);
            docRequest.clear();
            
            responseData = trySerialize(response);
            response.clear();
        }
        catch (const std::exception& e)
        {
            uint8_t endSignal = 0xFF;
            characteristic->setValue(&endSignal, sizeof(endSignal));
            log_e("[LOG][BLE:WRITE][E]: %s", e.what());
        }
    }

    void onRead(NimBLECharacteristic* characteristic) {
        if(!onReadCallback || isReceivingMessage){
            return;
        }

        try {
            JsonDocument  respDoc;
            JsonDocument  oldValue = tryDesserialize("{}");
            respDoc = onReadCallback(&oldValue);
            oldValue.clear();
        
            String data;
            serializeJson(respDoc, data);

            notify(characteristic, data);
            
            data.clear();
        
            respDoc.clear();
        }
        catch (const std::exception& e)
        {
            uint8_t endSignal = 0xFF;
            characteristic->setValue(&endSignal, sizeof(endSignal));                                                                    
            log_e("[LOG][BLE:READ][E]: %s", e.what());
        }
    }
    void onNotify(NimBLECharacteristic* characteristic) {
        if(isReceivingMessage){
            return;
        }
        // Serial.printf("[LOG][BLE:NOTIFY]: %s\r\n", characteristic->getValue().c_str());
    }

    void sendBLE(NimBLECharacteristic* characteristic, String& value){
        size_t offset = 0;  
        size_t dataLength = value.length() + 1;

        try {
            while (offset < dataLength) {
                size_t chunkSize = static_cast<size_t>(MAX_SIZE);
                
                string chunckStr = value.substring(offset, offset + chunkSize).c_str();
                characteristic->setValue(chunckStr);

                characteristic->notify();

                offset += chunkSize;
                chunckStr.clear();
            }
        }
        catch (const std::exception& e)
        {
            log_e("[LOG][BLE:READ][E]: %s", e.what());
        }
        
        uint8_t endSignal = 0xFF;
        characteristic->setValue(&endSignal, sizeof(endSignal));
        characteristic->notify();
    }
 
    JsonDocument  tryDesserialize(const char*  data){
        JsonDocument doc;

        DeserializationError error = deserializeJson(doc, data);
        
        if (error) {
            throw std::runtime_error("Falha na desserialização");
        }

        return doc;
    }
    String trySerialize(JsonDocument &doc){
        String data;
        serializeJson(doc, data);
        
        return data;
    }
public:
    BluetoothCallback();
    ~BluetoothCallback();

    std::function<JsonDocument (JsonDocument *)> onReadCallback; 
    std::function<JsonDocument (JsonDocument *)> onWriteCallback;

    void notify(NimBLECharacteristic* characteristic, String& value);
};

BluetoothCallback::BluetoothCallback() 
{
    // xSemaphoreGive(isNotify);
}

BluetoothCallback::~BluetoothCallback()
{
}

void BluetoothCallback::notify(NimBLECharacteristic* characteristic, String &value)
{
    sendBLE(characteristic, value);
}
#endif