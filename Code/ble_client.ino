#include <NimBLEDevice.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "secret.h"


static const NimBLEAdvertisedDevice* advDevice;
static bool doConnect = false;
static uint32_t scanTimeMs = 5000;  // Scan duration in milliseconds 0 = forever

const char* mqtt_server = "neptune4";
const int mqtt_port = 1883;
const char* mqtt_topic = "noBump/battery";
WiFiClient espClient;
PubSubClient client(espClient);


constexpr char UUID_NOBUMP_SERVICE[] = "AFFE";
constexpr char UUID_DOORSTATE_CHAR[] = "BEEF";

#define D1 1
#define D2 2
#define D7 17
#define D8 19
#define CONFIG_BT_NIMBLE_ROLE_BROADCASTER_DISABLED
#define CONFIG_BT_NIMBLE_ROLE_OBSERVER_DISABLED
#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS 1
#define CONFIG_BT_NIMBLE_MAX_CCCDS 1
#define CONFIG_BT_NIMBLE_MAX_SERVICES 1
#define CONFIG_BT_NIMBLE_MAX_CHARACTERISTICS 1
#define CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU 23

#ifdef DEBUG
#define DBG(x) Serial.println(x)
#else
#define DBG(x) do {} while (0)
#endif


enum DoorState : uint8_t {
  OPEN,
  CLOSED,
  ERROR
};

class ClientCallbacks : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient* pClient, int reason) override {
    DBG("%s Disconnected, reason = %d - Starting scan\n");
    NimBLEDevice::getScan()->start(scanTimeMs);
  }
} clientCallbacks;

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    if (d->isAdvertisingService(UUID_NOBUMP_SERVICE)) {
      NimBLEDevice::getScan()->stop();
      advDevice = d;
      doConnect = true;
    }
  }
};

void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (length < 1) {
    DBG("Received data length is less than 1, ignoring\n");
    return;
  }
  if (length == 4 && memcmp(pData, "OPEN", 4) == 0) {
    toggleLEDsFromDoorState(DoorState::OPEN);
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
        DBG("Reconnect failed\n");
        return false;
      }
      DBG("Reconnected client\n");
    } else {
      pClient = NimBLEDevice::getDisconnectedClient();
    }
  }
  if (!pClient) {
    if (NimBLEDevice::getCreatedClientCount() >= MYNEWT_VAL(BLE_MAX_CONNECTIONS)) {
      DBG("Max clients reached - no more connections available\n");
      return false;
    }
    pClient = NimBLEDevice::createClient();
    DBG("New client created\n");
    pClient->setClientCallbacks(&clientCallbacks, false);
    pClient->setConnectionParams(12, 12, 0, 150);
    pClient->setConnectTimeout(5 * 1000);
    if (!pClient->connect(advDevice)) {
      NimBLEDevice::deleteClient(pClient);
      DBG("Failed to connect, deleted client\n");
      return false;
    }
  }
  if (!pClient->isConnected()) {
    if (!pClient->connect(advDevice)) {
      DBG("Failed to connect\n");
      return false;
    }
  }
  DBG("Connected to Device");

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
        DBG("Subscribe failed\n");
        return false;
      }
      DBG("Subscribed Characteristic\n");
    }
  } else {
    DBG("DEAD service not found.\n");
  }
  DBG("Done with this device!\n");
  return true;
}

// Verbindung herstellen
void connectMQTT() {
  unsigned long startTime = millis();
  while (!client.connected() && millis() - startTime < 5000)  // Timeout 5 Sekunden
  {
    DBG("Verbinde mit MQTT...");
    if (client.connect("ESP32C6_Client")) {
      DBG("verbunden!");
    } else {
      DBG("Fehler, rc=");
      DBG(client.state());
      DBG(" -> Neuer Versuch in 1 Sekunde");
      delay(1000);
    }
  }
  if (!client.connected()) {
    DBG("MQTT Verbindung fehlgeschlagen, überspringe Publish.");
  }
}

void mqttPublishBattery() {
  // 1. Akku messen (Beispiel: ADC-Pin)
  //uint8_t percentage;
  //float vBat;
  //vBat = getVbatt();
  //percentage = mapFloat(vBat, minVoltage, maxVoltage);
  //DBG("Battery: %.2fV (%d%%)\n", vBat, percentage);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) return;

  client.setServer(mqtt_server, mqtt_port);
  if (client.connect("c6")) {
    char buf[4];
    itoa(50, buf, 10);
    client.publish(mqtt_topic, buf, true);
    client.disconnect();
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void toggleLEDsFromDoorState(DoorState state) {
  if (state == DoorState::OPEN) {
    DBG("Turning LEDs On");
    digitalWrite(D8, HIGH);
    digitalWrite(D1, HIGH);
    digitalWrite(D2, HIGH);
    digitalWrite(D7, HIGH);
  } else if (state == DoorState::CLOSED) {
    digitalWrite(D8, LOW);
    digitalWrite(D1, LOW);
    digitalWrite(D2, LOW);
    digitalWrite(D7, LOW);
  }
}

void setup() {
  Serial.begin(115200);
  DBG("Starting NimBLE Client\n");
  NimBLEDevice::init("NoBump-Client");

  NimBLEDevice::setPower(3);
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(&scanCallbacks, false);
  pScan->setInterval(100);
  pScan->setWindow(100);
  pScan->setActiveScan(false);
  pScan->start(scanTimeMs);
  DBG("Scanning for peripherals\n");

  // Pins der LEDs als Output setzen
  pinMode(D8, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D7, OUTPUT);

  // Pin für Spannungsmessung als Input setzen
  //pinMode(A0, INPUT);
}

void loop() {
  delay(10);

  if (doConnect) {
    doConnect = false;
    if (connectToServer()) {
      DBG("Success! we should now be getting notifications");
    } else {
      DBG("Failed to connect, starting scan");
      NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }
  }
}