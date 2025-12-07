#include <NewPing.h>
#include <stdbool.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>

#define TRIG_PIN 19
#define ECHO_PIN 17
#define MAX_DISTANCE 200
#define DOOR_CLOSED_THREASH 4


NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);
// MAC-Adresse des Empfängers (anpassen!)
uint8_t receiverMac[] = {0x10, 0x51, 0xDB, 0x1A, 0xE6, 0xC8};

// BLE advertising helper
BLEAdvertising *pAdvertising = nullptr;

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
  // BLE initialisieren
  BLEDevice::init("NoBumpSender");
  pAdvertising = BLEDevice::getAdvertising();
  Serial.println("BLE Advertiser initialisiert.");
}

void loop() {
  //Distanz messen
  uint32_t distance = sonar.ping_cm();

  if(distance == 0){
    Serial.println("Fehler bei der Distanzmessung");
    delay(1000);
    return;
  }

  //nur wenn sich der zustand geändert hat, nachricht verpacken und senden
  DoorState newState = getStateFromDistance(distance);
  if(newState == currentState){
    Serial.println("Zustand unverändert, sende keine Nachricht.");
    delay(1000);
    return;
  }

  currentState = newState;

  //auf Doorstate prüfen und in Nachricht verpacken
  message.state = currentState;

  Serial.print("Gemessene Distanz: "); Serial.print(distance); Serial.print(" cm, neuer Zustand: ");
  Serial.println(currentState == DoorState::OPEN ? "OPEN" : (currentState == DoorState::CLOSED ? "CLOSED" : "ERROR"));
  // Daten senden via BLE Advertising: wir packen ein Byte in Manufacturer Data
  uint8_t stateByte = message.state == DoorState::OPEN ? 2 : (message.state == DoorState::CLOSED ? 1 : 0);
  BLEAdvertisementData advData;
  std::string mdata(1, (char)stateByte);
  advData.setManufacturerData(mdata);
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->start();
  Serial.println("BLE Advertising gestartet");
  // kurz werben, dann stoppen (verringert Dauerverbrauch)
  delay(1000);
  pAdvertising->stop();
  Serial.println("BLE Advertising gestoppt");

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



