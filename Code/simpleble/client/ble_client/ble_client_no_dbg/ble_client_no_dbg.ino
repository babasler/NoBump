#include <NimBLEDevice.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "secret.h"


static const NimBLEAdvertisedDevice* advDevice;
static bool doConnect = false;
static uint32_t scanTimeMs = 5000;  // Scan duration in milliseconds 0 = forever

static NimBLEUUID UUID_NOBUMP_SERVICE("AFFE");
static NimBLEUUID UUID_DOORSTATE_CHAR("BEEF");

const uint8_t ledPins[] = { 1, 2, 17, 19 };

enum DoorState : uint8_t {
  OPEN,
  CLOSED
};

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    if (d->isAdvertisingService(UUID_NOBUMP_SERVICE)) {
      NimBLEDevice::getScan()->stop();
      advDevice = d;
      doConnect = true;
    }
  }
};
ScanCallbacks scanCallbacks;

void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (length < 1) {
    return;
  }
  if (length == 4 && memcmp(pData, "OPEN", 4) == 0) {
    toggleLEDsFromDoorState(DoorState::OPEN);
    mqttPublishBattery();
  } else if (length == 6 && memcmp(pData, "CLOSED", 6) == 0) {
    toggleLEDsFromDoorState(DoorState::CLOSED);
  }
}
bool connectToServer() {
  NimBLEClient* pClient = nullptr;
  if (NimBLEDevice::getCreatedClientCount()) {
    pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
    if (pClient) {
      if (!pClient->connect(advDevice, false)) {
        return false;
      }

    } else {
      pClient = NimBLEDevice::getDisconnectedClient();
    }
  }
  if (!pClient) {
    if (NimBLEDevice::getCreatedClientCount() >= MYNEWT_VAL(BLE_MAX_CONNECTIONS)) {
      return false;
    }
    pClient = NimBLEDevice::createClient();
    pClient->setConnectionParams(12, 12, 0, 150);
    pClient->setConnectTimeout(5 * 1000);
    if (!pClient->connect(advDevice)) {
      NimBLEDevice::deleteClient(pClient);
      return false;
    }
  }
  if (!pClient->isConnected()) {
    if (!pClient->connect(advDevice)) {
      return false;
    }
  }

  NimBLERemoteService* pSvc = nullptr;
  NimBLERemoteCharacteristic* pChr = nullptr;

  pSvc = pClient->getService(UUID_NOBUMP_SERVICE);
  if (pSvc) {
    pChr = pSvc->getCharacteristic(UUID_DOORSTATE_CHAR);
  }
  if (pChr) {
    if (pChr->canNotify()) {
      if (!pChr->subscribe(true, notifyCB)) {
        pClient->disconnect();
        return false;
      }
    }
  }
  return true;
}

void mqttPublishBattery() {
  // 1. Akku messen (Beispiel: ADC-Pin)
  //uint8_t percentage;
  //float vBat;
  //vBat = getVbatt();
  //percentage = mapFloat(vBat, minVoltage, maxVoltage);
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


void toggleLEDsFromDoorState(DoorState state) {
  for (uint8_t i = 0; i < 4; i++) digitalWrite(ledPins[i], state == DoorState::OPEN ? HIGH : LOW);
}

void setup() {
  NimBLEDevice::init("NoBump-Client");

  NimBLEDevice::setPower(3);
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(&scanCallbacks, false);
  pScan->setInterval(100);
  pScan->setWindow(100);
  pScan->setActiveScan(false);
  pScan->start(scanTimeMs);

  // Pins der LEDs als Output setzen
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(17, OUTPUT);
  pinMode(19, OUTPUT);

  // Pin für Spannungsmessung als Input setzen
  //pinMode(A0, INPUT);
}

void loop() {
  delay(10);

  if (doConnect) {
    doConnect = false;
    if (!connectToServer()) {
      NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }
  }
}