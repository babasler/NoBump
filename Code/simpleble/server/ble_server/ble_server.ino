#include <NimBLEDevice.h>
#include <NewPing.h>
//Er schreibt auf die Characteristic, deswegen ist das der Sender -> Misst den Doorstate

#define UUID_NOBUMP_SERVICE      "12345678-0001-0000-0000-000000000001"
#define UUID_DOORSTATE_CHAR      "12345678-0001-0000-0000-000000000002"
#define TRIG_PIN 19
#define ECHO_PIN 17
#define MAX_DISTANCE 200
#define DOOR_CLOSED_THREASH 4
#define DEBUG

static NimBLEServer* pServer;
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);

enum class DoorState{
  OPEN,
  CLOSED,
  ERROR
};

DoorState getStateFromDistance(uint32_t distance);

// aktueller zustand
DoorState currentState;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        #ifdef DEBUG
        Serial.printf("Client address: %s\n", connInfo.getAddress().toString().c_str());
        #endif
        pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 180);
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        #ifdef DEBUG
        Serial.printf("Client disconnected - start advertising\n");
        #endif
        NimBLEDevice::startAdvertising();
    }
} serverCallbacks;

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

void setup(void) {
    #ifdef DEBUG
    Serial.begin(115200);
    Serial.printf("Starting NoBump Server\n");
    #endif

    NimBLEDevice::init("NoBump");

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);

    NimBLEService*        pDeadService = pServer->createService(UUID_NOBUMP_SERVICE);
    NimBLECharacteristic* pBeefCharacteristic =
        pDeadService->createCharacteristic(UUID_DOORSTATE_CHAR, NIMBLE_PROPERTY::NOTIFY 
                                          
        );
    pDeadService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setName("NoBump-Server");
    pAdvertising->addServiceUUID(pDeadService->getUUID());
    pAdvertising->enableScanResponse(false);
    pAdvertising->start();
    #ifdef DEBUG
    Serial.printf("Advertising Started\n");
    #endif
}

void loop() {
delay(2000);

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
#ifdef DEBUG
Serial.print("Gemessene Distanz: "); Serial.print(distance); Serial.print(" cm, neuer Zustand: ");
Serial.println(currentState == DoorState::OPEN ? "OPEN" : (currentState == DoorState::CLOSED ? "CLOSED" : "ERROR"));
#endif

if (pServer->getConnectedCount()) {
    NimBLEService* pSvc = pServer->getServiceByUUID(UUID_NOBUMP_SERVICE);
    if (pSvc) {
        NimBLECharacteristic* pChr = pSvc->getCharacteristic(UUID_DOORSTATE_CHAR);
        if (pChr) {
            pChr->setValue(currentState == DoorState::OPEN ? "OPEN" : (currentState == DoorState::CLOSED ? "CLOSED" : "ERROR"));
            pChr->notify();
        }
    }
}
}