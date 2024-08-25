#ifndef BLUETOOTH_CALLBACK
#define BLUETOOTH_CALLBACK

#include <AsyncJson.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>


class BluetoothCallback : public BLECharacteristicCallbacks 
{
private:
    void onWrite(BLECharacteristic* characteristic) {
        if(!onWriteCallback) return;

        const char* value = characteristic->getValue().c_str();
        if (sizeof(value) > 0) {
            Serial.printf("Mensagem recebida: %s\r\n", value);
        }
        DynamicJsonDocument doc = tryDesserialize(value);
        onWriteCallback(&doc);
    }

    void onRead(BLECharacteristic* characteristic) {
        Serial.printf("READ");
        if(!onReadCallback) return;
        
        DynamicJsonDocument doc = onReadCallback(nullptr);

        const char*  value = trySerialize(&doc);

        characteristic->setValue(value);

        if (sizeof(value) > 0) {
            Serial.printf("Mensagem enviada: %s\r\n", value);
        }
        delete value;
    }
    
    DynamicJsonDocument tryDesserialize(const char*  data){
        DynamicJsonDocument doc(sizeof(data));
        DeserializationError error = deserializeJson(doc, data);
        
        if (error) {
            throw("Falha na desserialização: %s" + String(error.f_str()));
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

    void notify(DynamicJsonDocument *doc);
    void setValue(DynamicJsonDocument *doc);
};

BluetoothCallback::BluetoothCallback() 
{
}

BluetoothCallback::~BluetoothCallback()
{
}

void BluetoothCallback::notify(DynamicJsonDocument *doc)
{
    // pCharacteristic->setValue(trySerialize(doc));
    // pCharacteristic->notify();
}
void BluetoothCallback::setValue(DynamicJsonDocument *doc)
{
    // pCharacteristic->setValue(trySerialize(doc));
}





#endif