#include <NewPing.h>
#include <stdbool.h>
#include <NimBLEDevice.h>

#define TRIG_PIN 19
#define ECHO_PIN 17
#define MAX_DISTANCE 200
#define DOOR_CLOSED_THREASH 4
#define DEBUG


NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);

// BLE advertising helper (NimBLE)
NimBLEAdvertising *pAdvertising = nullptr;
NimBLEServer *pServer = nullptr;
NimBLEService *pService = nullptr;
NimBLECharacteristic *pCharDoor = nullptr;

// GATT UUIDs (beliebig, aber stabil halten)
static const char *SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char *CHAR_DOOR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // Notify

enum class DoorState{
  OPEN,
  CLOSED,
  ERROR
};

typedef struct command_message {
  DoorState state;
} command_message;

// Datenstruktur erstellen
command_message message;

DoorState getStateFromDistance(uint32_t distance);

// aktueller zustand
DoorState currentState;

void setup() {
  Serial.begin(115200);
  Serial.println("Sender gestartet");
  // BLE initialisieren (NimBLE)
  NimBLEDevice::init("NoBumpSender");
  // GATT Server + Service + Characteristic (Notify)
  pServer = NimBLEDevice::createServer();
  pService = pServer->createService(SERVICE_UUID);
  pCharDoor = pService->createCharacteristic(CHAR_DOOR_UUID, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ);
  pCharDoor->setValue((uint8_t)0); // initialer Zustand
  pService->start();

  // Advertising mit Service UUID, damit der Client uns findet
  pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  // NimBLEAdvertising hat kein setScanResponse(bool); optional könnten wir Scan-Response-Daten setzen.
  pAdvertising->start();
  Serial.println("BLE Advertiser initialisiert.");
}

void loop() {
  //Distanz messen
  uint32_t distance = sonar.ping_cm();

  if(distance == 0){
    #ifdef DEBUG
    Serial.println("Fehler bei der Distanzmessung");
    #endif
    delay(1000);
    return;
  }

  //nur wenn sich der zustand geändert hat, nachricht verpacken und senden
  DoorState newState = getStateFromDistance(distance);
  if(newState == currentState){
    #ifdef DEBUG
    Serial.println("Zustand unverändert, sende keine Nachricht.");
    #endif
    delay(1000);
    return;
  }

  currentState = newState;

  //auf Doorstate prüfen und in Nachricht verpacken
  message.state = currentState;
  #ifdef DEBUG
  Serial.print("Gemessene Distanz: "); Serial.print(distance); Serial.print(" cm, neuer Zustand: ");
  Serial.println(currentState == DoorState::OPEN ? "OPEN" : (currentState == DoorState::CLOSED ? "CLOSED" : "ERROR"));
  #endif
  // Notify den neuen Zustand über GATT Characteristic
  uint8_t stateByte = message.state == DoorState::OPEN ? 2 : (message.state == DoorState::CLOSED ? 1 : 0);
  pCharDoor->setValue(&stateByte, 1);
  pCharDoor->notify(true);
  Serial.println("BLE GATT Notify gesendet");

  delay(2000);  // alle 2 Sekunden prüfen
}

DoorState getStateFromDistance(uint32_t distance){
  if(distance > DOOR_CLOSED_THREASH){
    return DoorState::OPEN;
  }
  else if(distance <= DOOR_CLOSED_THREASH){
    return DoorState::CLOSED;
  }
  else{
    return DoorState::OPEN; //default fallback
  }
}



