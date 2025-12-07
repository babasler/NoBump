#include <esp_now.h>
#include <WiFi.h>
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

// Callback, der ausgelöst wird, wenn Daten empfangen werden
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
  memcpy(&message, data, sizeof(message));

#ifdef DEBUG
  Serial.print("Daten empfangen von: ");
  Serial.print("Sender MAC: ");
  for (int i = 0; i < 6; i++)
  {
    Serial.printf("%02X", info->src_addr[i]);
    if (i < 5)
      Serial.print(":");
  }

  Serial.print(" Doorstate: ");
  Serial.println(message.state == DoorState::OPEN ? "OPEN" : (message.state == DoorState::CLOSED ? "CLOSED" : "ERROR"));
#endif

  toggleLEDsFromDoorState(message.state);
  mqttPublishBattery();
}

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

void scanNetworks()
{
  Serial.println("Netzwerkscan gestartet...");
  int n = WiFi.scanNetworks();
  Serial.println("Scan abgeschlossen.");
  if (n == 0)
  {
    Serial.println("Keine Netzwerke gefunden.");
  }
  else
  {
    Serial.print(n);
    Serial.println(" Netzwerke gefunden:");
    for (int i = 0; i < n; ++i)
    {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(" dBm) ");
      Serial.print((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Offen" : "Verschlüsselt");
      Serial.println();
    }
  }
}

void setup()
{
  Serial.begin(115200);

  // ESP-NOW zuerst initialisieren
  WiFi.mode(WIFI_STA); // ESP-NOW braucht STA-Modus
  scanNetworks();
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Fehler beim Start von ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);

  // Pins der LEDs als Output setzen
  pinMode(D8, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D7, OUTPUT);

  // Pin für Spannungsmessung als Input setzen
  pinMode(A0, INPUT);

#ifdef DEBUG
  Serial.println("ESPNow Empfänger gestartet.");
#endif
}

void loop()
{
  // Keine Schleifenlogik nötig – alles läuft über Callback
}
