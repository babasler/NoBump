#include <NimBLEDevice.h>
#include <NewPing.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <secret.h>
//Er schreibt auf die Characteristic, deswegen ist das der Sender -> Misst den Doorstate

#define UUID_NOBUMP_SERVICE      "AFFE"
#define UUID_DOORSTATE_CHAR      "BEEF"
#define UUID_BATTERY_LEVEL_CHAR  
#define TRIG_PIN 19
#define ECHO_PIN 17
#define MAX_DISTANCE 200
#define DOOR_CLOSED_THREASH 4

#ifdef DEBUG
#define DBG(x) Serial.println(x)
#else
#define DBG(x) do {} while (0)
#endif

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
        pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 180);
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        DBG("Client disconnected - start advertising\n");
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
    return DoorState::OPEN;
}
}

void mqttPublishBattery(uint8_t battery_level) {
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
    delay(100);
  }

  static WiFiClient espClient;
  static PubSubClient client(espClient);

  if (WiFi.status() != WL_CONNECTED) return;  // Verbindung fehlgeschlagen

  client.setServer("neptune4", 1883);

  if (client.connect("c6")) {
    char buf[4];
    itoa(50, buf, 10);
    client.publish("noBump/battery", buf, true);
    client.disconnect();
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void setup(void) {
    Serial.begin(115200);
    DBG("Starting NoBump Server\n");

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
    DBG("Advertising Started\n");
}

void loop() {
delay(400);

//Distanz messen
uint32_t distance = sonar.ping_cm();
  
if(distance == 0){
    DBG("Fehler bei der Distanzmessung");
    delay(1000);
    return;
}

  //nur wenn sich der zustand geändert hat, nachricht verpacken und senden
DoorState newState = getStateFromDistance(distance);
if(newState == currentState){
    DBG("Zustand unverändert, sende keine Nachricht.");
    delay(1000);
    return;
  }

currentState = newState;
DBG(currentState == DoorState::OPEN ? "OPEN" : (currentState == DoorState::CLOSED ? "CLOSED" : "ERROR"));

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