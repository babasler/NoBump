#include <NimBLEDevice.h>
#include <NewPing.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <secret.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const NimBLEAdvertisedDevice* advDevice;
TaskHandle_t wifiTaskHandle = NULL;
static bool doConnect = false;
static uint32_t scanTimeMs = 5000;  // Scan duration in milliseconds 0 = forever

static NimBLEUUID UUID_NOBUMP_SERVICE("AFFE");
static NimBLEUUID UUID_DOORSTATE_CHAR("BEEF");

volatile bool doorStateProcessing = false;  // Flag für DoorState-Priorität

const uint8_t ledPins[] = { 1, 2, 17, 20 };
#define DEBUG

#ifdef DEBUG
#define DBG(x) Serial.println(x)
#else
#define DBG(x) do {} while (0)
#endif

enum DoorState : uint8_t {
  OPEN,
  CLOSED
};

struct TaskParams {
  PubSubClient* mqttClient;
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
    DBG("Revieved Open State");
    toggleLEDsFromDoorState(DoorState::OPEN);
  } else if (length == 6 && memcmp(pData, "CLOSED", 6) == 0) {
    DBG("Revieved Closed State");
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

void sendBatteryTask(void *parameter) {
  TaskParams* params = (TaskParams*)parameter;
  PubSubClient* mqttClient = params->mqttClient;
  uint8_t i = 1;
  mqttClient->setServer("neptune4", 1883);
  for (;;) {
    // Warten auf BLE-Trigger
    vTaskDelay(pdMS_TO_TICKS(5 * 60 * 1000)); // 5 Minuten warten
    DBG("Waking up");
     if (!doorStateProcessing) {  // nur wenn keine DoorState-Bearbeitung läuft
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      unsigned long start = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        vTaskDelay(pdMS_TO_TICKS(50));
      }

      if (WiFi.status() == WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500)); // 0,5 Sekunden Stabilisierung
        if (mqttClient->connect("c6")) {
          vTaskDelay(pdMS_TO_TICKS(100));
          char buf[4];
          itoa(i, buf, 10);  // Beispielwert
          mqttClient->publish("noBump/battery", buf, true);
          i++;
          DBG("publish done");
          vTaskDelay(pdMS_TO_TICKS(300)); // 100ms für TCP
          mqttClient->disconnect();
        }
      }

      WiFi.disconnect(false);
      WiFi.mode(WIFI_OFF);
      DBG("Bye Bye");
    }
  }
}

void toggleLEDsFromDoorState(DoorState state) {
  doorStateProcessing = true;
  for (uint8_t i = 0; i < 4; i++) digitalWrite(ledPins[i], state == DoorState::OPEN ? HIGH : LOW);
  doorStateProcessing = false;
}

void setup() {
  NimBLEDevice::init("NoBump-Client");

  Serial.begin(115200);

  NimBLEDevice::setPower(3);
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(&scanCallbacks, false);
  pScan->setInterval(100);
  pScan->setWindow(100);
  pScan->setActiveScan(false);
  pScan->start(scanTimeMs);

  //WIFI Config
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);           
  WiFi.setAutoReconnect(true);

  static WiFiClient wifiClient;
  static PubSubClient mqttClient(wifiClient);

  static TaskParams params = {&mqttClient };

  //Thread für mqtt
  xTaskCreatePinnedToCore(
  sendBatteryTask,
  "WiFiTask",
  6144,
  &params,
  1,
  &wifiTaskHandle,
  0  
  );
  for (uint8_t i = 0; i < 4; i++) pinMode(ledPins[i], OUTPUT);

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