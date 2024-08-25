// #ifndef BLUETOOTH_SERVICE
// #define BLUETOOTH_SERVICE

// #include <AsyncJson.h>

// #include <BLEDevice.h>
// #include <BLEServer.h>
// #include <BLE2902.h>
// #include <BLE2902.h>

// #include "Bluetooth/BluetoothCallback.h"

// class BluetoothService
// {
// private:
//     BLEServer *pServer = NULL;
//     BLEService *pService;
     
// public:
//     BluetoothService();
//     ~BluetoothService();
//     void addService();
    
//     BLECharacteristic* addCharacteristics(std::string characteristicUUID, 
//                             std::function<DynamicJsonDocument()> onReadCallback,
//                             std::function<void(DynamicJsonDocument)> onWriteCallback, 
//                             bool onNotifyCallback);
//     void startService(BLEServer* server);
// };

// BluetoothService::BluetoothService()
// {  
//     // pService = pServer->createService(_uuid);
// }

// BluetoothService::~BluetoothService()
// {
// }

// BLECharacteristic* BluetoothService::addCharacteristics(
//     std::string characteristicUUID, 
//                             std::function<DynamicJsonDocument()> onReadCallback,
//                             std::function<void(DynamicJsonDocument)> onWriteCallback, 
//                             bool onNotifyCallback)
// {
//     BluetoothCallback *bleCallback;
//     bleCallback->onReadCallback = onReadCallback;
//     bleCallback->onWriteCallback = onWriteCallback;

//     uint32_t properties;
//     if (onReadCallback != nullptr) 
//        properties |= BLECharacteristic::PROPERTY_READ;
       
//     if (onWriteCallback != nullptr) 
//        properties |= BLECharacteristic::PROPERTY_WRITE;

//     if (onNotifyCallback) 
//        properties |= BLECharacteristic::PROPERTY_NOTIFY;

//     BLECharacteristic *caracteristicas = pService->createCharacteristic(characteristicUUID, properties);
//     caracteristicas->setCallbacks(bleCallback);
//     return caracteristicas;
// }

// void BluetoothService::startService(BLEServer* server){
//     // BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
//     // pAdvertising->addServiceUUID(_uuid);
//     // pAdvertising->start();
//     pServer = server;
//     // pService = pServer->createService(_uuid);
//     pService->start();
// }
// #endif