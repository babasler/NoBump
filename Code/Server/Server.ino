/*
    Der Server ist bei Arduino, wohl derjenige, der die Befehle Empfängt.
    Er stellt eine Charakteristic bereit und der Client schreibt auf diese.
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <string.h>

using namespace std;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define DOOR_OPEN "OPEN"
#define DOOR_CLOSED "CLOSED"

#define LED_PIN 2
#define TICKS 5
#define DELAY_MS 50

#define OK 0
#define UNKNOWN_COMMAND -1

BLECharacteristic *pCharacteristic;

void testLed(uint8_t pin, uint8_t ticks);
int8_t turnOnWarnSignalIfNessecary(String signal);


class serverCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String value = pCharacteristic->getValue();
        if (value.length() > 0) {
            Serial.print("Befehl empfangen: ");
            Serial.println(value.c_str());
            int8_t ret = turnOnWarnSignalIfNessecary(value);
            if(ret < 0){
              Serial.println("Recieved unkown command");
            }
        }
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  pinMode(LED_PIN,OUTPUT);

  BLEDevice::init("NoBump Server");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  pCharacteristic->setCallbacks(new serverCallbacks());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined");

  //Test LED
  Serial.println("Testing LED now");
  testLed(LED_PIN,TICKS);

}

void loop() {
  delay(2000);
}

void testLed(uint8_t pin, uint8_t ticks){
  for(uint8_t i = 0; i<ticks; i++){
    digitalWrite(pin,HIGH);
    delay(1000);
    digitalWrite(pin,LOW);
    delay(1000);
  }
}

int8_t turnOnWarnSignalIfNessecary(String signal){
  if(signal == DOOR_OPEN){
    digitalWrite(LED_PIN,HIGH);
    delay(DELAY_MS);
    return OK;
  }
  else if(signal == DOOR_CLOSED){
    digitalWrite(LED_PIN,LOW);
    delay(DELAY_MS);
    return OK;
  }
  else{
    return UNKNOWN_COMMAND;
  }

}

