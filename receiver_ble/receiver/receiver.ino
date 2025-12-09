#include <WiFi.h>
#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <NimBLEClient.h>
#include <NimBLEAdvertisedDevice.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "secret.h"

#define D1 1
#define D2 2
#define D7 17
#define D8 19

#define DEBUG

enum class DoorState
{
  OPEN,
  CLOSED,
  ERROR
};

typedef struct command_message
{
  DoorState state;
} command_message;

// Datenstruktur erstellen
command_message message;

// 3.7 V Li-Ion battery voltage
const float minVoltage = 3.0;
const float maxVoltage = 4.0;

// WLAN und MQTT
const char *mqtt_server = "neptune4";
const int mqtt_port = 1883;
const char *mqtt_topic = "noBump/battery";
WiFiClient espClient;
PubSubClient client(espClient);

// Verbindung herstellen
void connectMQTT()
{
  unsigned long startTime = millis();
  while (!client.connected() && millis() - startTime < 5000) // Timeout 5 Sekunden
  {
#ifdef DEBUG
    Serial.print("Verbinde mit MQTT...");
#endif
    if (client.connect("ESP32C6_Client"))
    {
#ifdef DEBUG
      Serial.println("verbunden!");
#endif
    }
    else
    {
#ifdef DEBUG
      Serial.print("Fehler, rc=");
      Serial.print(client.state());
      Serial.println(" -> Neuer Versuch in 1 Sekunde");
#endif
      delay(1000);
    }
  }

  if (!client.connected())
  {
#ifdef DEBUG
    Serial.println("MQTT Verbindung fehlgeschlagen, überspringe Publish.");
#endif
  }
}

// GATT UUIDs müssen zur Sender-Seite passen
static const char *SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char *CHAR_DOOR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";

NimBLEScan *pBLEScan = nullptr;
NimBLEClient *pClient = nullptr;
NimBLERemoteService *pRemoteService = nullptr;
NimBLERemoteCharacteristic *pRemoteCharDoor = nullptr;

// Notification-Callback vom GATT-Server (Sender)
void onDoorNotify(NimBLERemoteCharacteristic* chr, uint8_t* data, size_t length, bool isNotify) {
  if (length < 1) return;
  uint8_t stateByte = data[0];
  DoorState receivedState = DoorState::ERROR;
  if (stateByte == 1) receivedState = DoorState::CLOSED;
  else if (stateByte == 2) receivedState = DoorState::OPEN;

#ifdef DEBUG
  Serial.print("GATT Notify: DoorState = ");
  Serial.println(receivedState == DoorState::OPEN ? "OPEN" : (receivedState == DoorState::CLOSED ? "CLOSED" : "ERROR"));
#endif

  toggleLEDsFromDoorState(receivedState);
  mqttPublishBattery();
}

// Scan-Callback: bei Gerät mit Service-UUID verbinden
class MyScanCallbacks : public NimBLEScanCallbacks {
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
#ifdef DEBUG
    Serial.print("Gefunden: "); Serial.println(advertisedDevice->getAddress().toString().c_str());
#endif
    if (!advertisedDevice->haveServiceUUID() || !advertisedDevice->isAdvertisingService(NimBLEUUID(SERVICE_UUID))) return;

    pBLEScan->stop();
    NimBLEAddress addr = advertisedDevice->getAddress();
    pClient = NimBLEDevice::createClient();
#ifdef DEBUG
    Serial.print("Verbinde zu "); Serial.println(addr.toString().c_str());
#endif
    if (!pClient->connect(addr)) {
      Serial.println("Verbindung fehlgeschlagen");
      pBLEScan->start(10, true);
      return;
    }
    pRemoteService = pClient->getService(SERVICE_UUID);
    if (!pRemoteService) {
      Serial.println("Service nicht gefunden");
      pClient->disconnect();
      pBLEScan->start(10, true);
      return;
    }
    pRemoteCharDoor = pRemoteService->getCharacteristic(CHAR_DOOR_UUID);
    if (!pRemoteCharDoor) {
      Serial.println("Characteristic nicht gefunden");
      pClient->disconnect();
      pBLEScan->start(10, true);
      return;
    }
    if (pRemoteCharDoor->canNotify()) {
      pRemoteCharDoor->subscribe(true, onDoorNotify);
      Serial.println("Notify abonniert");
    }
  }
};

void mqttPublishBattery()
{
  // 1. Akku messen (Beispiel: ADC-Pin)
  uint8_t percentage;
  float vBat;
  vBat = getVbatt();
  percentage = mapFloat(vBat, minVoltage, maxVoltage);
#ifdef DEBUG
  Serial.printf("Battery: %.2fV (%d%%)\n", vBat, percentage);
#endif

  // 2. WLAN nur für MQTT aktivieren
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20) // Timeout 20 msg
  {
    delay(10);
  }

  if (WiFi.status() != WL_CONNECTED)
  {
#ifdef DEBUG
    Serial.println("WLAN nicht verbunden, MQTT Publish übersprungen");
#endif
    return;
  }

  // 3. MQTT verbinden und senden
  client.setServer(mqtt_server, mqtt_port);
  connectMQTT();

  const char *payload = createJson(percentage).c_str();
  if (client.connected())
  {
    client.publish(mqtt_topic, payload);
#ifdef DEBUG
    Serial.printf("MQTT gesendet: %s\n", payload);
#endif
    client.disconnect();
  }

  // 4. WLAN wieder ausschalten
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

String createJson(int batteryLevel)
{
  StaticJsonDocument<200> doc;

  doc["battery_level"] = String(batteryLevel) + "%";

  // echte Zeit holen (falls NTP verwendet)
  time_t now = time(nullptr);
  char timeString[30];
  strftime(timeString, sizeof(timeString), "%FT%TZ", gmtime(&now));
  doc["ts"] = timeString;

  String output;
  serializeJson(doc, output);
  return output;
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

void toggleLEDsFromDoorState(DoorState state)
{
  if (state == DoorState::OPEN)
  {
    digitalWrite(D8, HIGH);
    digitalWrite(D1, HIGH);
    digitalWrite(D2, HIGH);
    digitalWrite(D7, HIGH);
  }
  else if (state == DoorState::CLOSED)
  {
    digitalWrite(D8, LOW);
    digitalWrite(D1, LOW);
    digitalWrite(D2, LOW);
    digitalWrite(D7, LOW);
  }
}

void setup()
{
  Serial.begin(115200);

  // BLE initialisieren
  Serial.println("Initialisiere BLE Client...");
  NimBLEDevice::init("NoBumpReceiver");
  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setScanCallbacks(new MyScanCallbacks());
  pBLEScan->setActiveScan(true); // aktives Scannen für schnellere Verbindung
  pBLEScan->start(10, true); // 10s, weiterlaufen

  // Pins der LEDs als Output setzen
  pinMode(D8, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D7, OUTPUT);

  // Pin für Spannungsmessung als Input setzen
  pinMode(A0, INPUT);

#ifdef DEBUG
  Serial.println("BLE Empfänger gestartet.");
#endif
}

void loop()
{
  // Keine Schleifenlogik nötig – alles läuft über Callback
}
