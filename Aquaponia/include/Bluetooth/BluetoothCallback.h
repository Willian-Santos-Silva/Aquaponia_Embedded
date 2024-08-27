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
        Serial.printf("WRITE");
        if(!onWriteCallback) return;

        try{
            const char* value = characteristic->getValue().c_str();
            if (sizeof(value) > 0) {
                Serial.printf("Mensagem recebida: %s\r\n", value);
            }
            DynamicJsonDocument doc = tryDesserialize(value);


            DynamicJsonDocument response = onWriteCallback(&doc);
            const char* repsValue = trySerialize(&response);
            characteristic->setValue(repsValue);
        }
        catch (const std::exception& e)
        {
            characteristic->setValue(e.what());
            Serial.printf("erro: %s\n", e.what());
        }
    }

    void onRead(BLECharacteristic* characteristic) {
        Serial.printf("READ");
        if(!onReadCallback) return;
        try {
            DynamicJsonDocument oldValue = tryDesserialize(characteristic->getValue().c_str());

            DynamicJsonDocument doc = onReadCallback(&oldValue);
            
            const char* value = trySerialize(&doc);

            characteristic->setValue(value);

            if (sizeof(value) > 0) {
                Serial.printf("Mensagem enviada: %s\r\n", value);
            }
        }
        catch (const std::exception& e)
        {
            characteristic->setValue(e.what());
            Serial.printf("erro: %s\n", e.what());
        }
        //delete value;
    }
    void onNotify(BLECharacteristic* characteristic) {
        Serial.printf("NOTIFY");
        std::string value = characteristic->getValue();
        if (value.length() > 0) {    
            Serial.printf("Mensagem notificada: %s\r\n", value.c_str());
        }

        // try{
        //     DynamicJsonDocument doc = tryDesserialize(value);
        //     onWriteCallback(&doc);
        // }
        // catch (const std::exception& e)
        // {
        //     Serial.printf("erro: %s\n", e.what());
        // }
    }

    
    DynamicJsonDocument tryDesserialize(const char*  data = "{}"){
        DynamicJsonDocument doc(sizeof(data) + 200);
        std::string value = data == "" ? "{}" : data;   

        DeserializationError error = deserializeJson(doc, value);
        
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