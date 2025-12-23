#include <NimBLEDevice.h>
#include <NewPing.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <secret.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_pm.h"
#include "esp_wifi.h"

bool connectToServer();

static const NimBLEAdvertisedDevice* advDevice;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t BLEConnectTaskHandle = NULL;
static uint32_t scanTimeMs = 5000;  // Scan duration in milliseconds 0 = forever

static NimBLEUUID UUID_NOBUMP_SERVICE("AFFE");
static NimBLEUUID UUID_DOORSTATE_CHAR("BEEF");

volatile bool doorStateProcessing = false;  // Flag für DoorState-Priorität
volatile bool doConnect = false;

// 3.7 V Li-Ion battery voltage
const float minVoltage = 3.0;
const float maxVoltage = 4.0;

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
    DBG("Found a Device");
    if (d->isAdvertisingService(UUID_NOBUMP_SERVICE)) {
      NimBLEDevice::getScan()->stop();
      advDevice = d;
      DBG("Server found -> connect");
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
class ClientCallbacks : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient* pClient) {
    DBG("BLE disconnected, restarting scan");
    NimBLEDevice::getScan()->start(scanTimeMs, false);
  }
};
static ClientCallbacks clientCB;
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
    pClient->setConnectionParams(80, 160, 4, 400);//nicht so aggressiv
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

  pClient->setClientCallbacks(&clientCB, false);

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
  mqttClient->setServer("neptune4", 1883);
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(10 * 60 * 1000)); // 10 Minuten warten
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
        if (mqttClient->connect("c6",NULL,NULL,NULL,0,false,NULL,true)) {
          vTaskDelay(pdMS_TO_TICKS(100));
          uint8_t percentage;
          float vBat;
          vBat = getVbatt();
          percentage = mapFloat(vBat, minVoltage, maxVoltage);
          char buf[4];
          itoa(percentage, buf, 10);
          DBG(percentage);
          mqttClient->publish("noBump/battery", buf, true);
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

void BLEConnectTask(void* parameter) {
    for (;;) {
        if (doConnect) {
            doConnect = false;
            DBG("Connecting To Server");
            connectToServer(); // CPU aktiv nur für kurze Zeit
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // sleep-freundlich
    }
}


float getVbatt()
{
  uint32_t Vbatt = 0;
  for (int i = 0; i < 16; i++)
  {
    Vbatt += analogReadMilliVolts(A0); // Read and accumulate ADC voltage
  }
  return (2 * Vbatt / 16 / 1000.0); // Adjust for 1:2 divider and convert to volts
}

uint8_t mapFloat(float x, float in_min, float in_max)
{
  float val;
  val = (x - in_min) * (100) / (in_max - in_min);
  if (val < 0)
  {
    val = 0;
  }
  else if (val > 100)
  {
    val = 100;
  }
  return (uint8_t)val;
}

void toggleLEDsFromDoorState(DoorState state) {
  doorStateProcessing = true;
  for (uint8_t i = 0; i < 4; i++) digitalWrite(ledPins[i], state == DoorState::OPEN ? HIGH : LOW);
  doorStateProcessing = false;
}

void setup() {
  NimBLEDevice::init("NoBump-Client");

  Serial.begin(115200);
  DBG("Starte jetzt");

  NimBLEDevice::setPower(0);
  esp_sleep_enable_bt_wakeup();
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(&scanCallbacks, false);
  pScan->setInterval(100);
  pScan->setWindow(100);
  pScan->setActiveScan(false);
  pScan->start(scanTimeMs);

  //WIFI Config
  WiFi.mode(WIFI_OFF);
  WiFi.setSleep(true);           

  static WiFiClient wifiClient;
  static PubSubClient mqttClient(wifiClient);

  static TaskParams params = {&mqttClient};

  //Thread für mqtt
  xTaskCreatePinnedToCore(sendBatteryTask,"WiFiTask",6144,&params,1,
  &wifiTaskHandle,0);
  xTaskCreatePinnedToCore(BLEConnectTask,"BLEConnectTast",4096,NULL,1,
  &BLEConnectTaskHandle,0);

  for (uint8_t i = 0; i < 4; i++) pinMode(ledPins[i], OUTPUT);

  // Pin für Spannungsmessung als Input setzen
  pinMode(A0, INPUT);

   esp_pm_config_t pm_config = {
    .max_freq_mhz = 160,
    .min_freq_mhz = 10,
    .light_sleep_enable = true
  };
  esp_pm_configure(&pm_config);
  DBG("Setup Ende");
}

void loop() {
}