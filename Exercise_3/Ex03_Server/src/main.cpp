#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define SERVICE_UUID "f7826da6-4fa2-4e98-8024-bc5b71e0893e"
#define CHARACTERISTIC_UUID "b9407f30-f5f8-466e-aff9-25556b57fe6d"

bool written = false;

BLECharacteristic *pCharacteristic;

uint8_t test_value[] = {0xA, 0xB, 0xC, 0xD};

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();
    Serial.print("Received ");
    Serial.print(value.length());
    Serial.println(" bytes");
    Serial.println(value.c_str());
    written = true;
  }
  void onRead(BLECharacteristic *pCharacteristic)
  {
    Serial.println("Read!");
  }
};

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    Serial.println("Connected!");
  }
};

void setup()
{
  Serial.begin(115200);

  BLEDevice::init("Emsec2019-Server1");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID,
                                                    BLECharacteristic::PROPERTY_READ 
                                                    | BLECharacteristic::PROPERTY_WRITE 
                                                    | BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic->setCallbacks(new MyCallbacks());

  pCharacteristic->addDescriptor(new BLE2902());

  pCharacteristic->setValue(test_value, 4);

  pServer->setCallbacks(new MyServerCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
}

void loop()
{
  if (written == true)
  {
    written = false;
    test_value[0]++;
    pCharacteristic->setValue(test_value, 4);
    pCharacteristic->notify();
  }
  delay(10);
}