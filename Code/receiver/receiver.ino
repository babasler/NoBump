#include <esp_now.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define D1 1
#define D2 2
#define D7 17
#define D8 19


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

// 3.7 V Li-Ion battery voltage
const float minVoltage = 3.0;
const float maxVoltage = 4.0;

// WLAN und MQTT 
const char* ssid = "DEIN_WIFI";
const char* password = "DEIN_PASS";
const char* mqtt_server = "test.mosquitto.org";
const int   mqtt_port = 1883;
const char* mqtt_topic = "esp32c6/battery";
WiFiClient espClient;
PubSubClient client(espClient)

// Callback, der ausgelöst wird, wenn Daten empfangen werden
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len){
  memcpy(&message, data, sizeof(message));
  
  #ifdef DEBUG
  Serial.print("Daten empfangen von: ");
  Serial.print("Sender MAC: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", info->src_addr[i]);
        if (i < 5) Serial.print(":");
    }

  Serial.print("Doorstate: "); Serial.println(message.state == DoorState::OPEN ? "OPEN" : (message.state == DoorState::CLOSED ? "CLOSED" : "ERROR"));
  #endif

  toggleLEDsFromDoorState(message.state);

  
  
}

// ---------- MQTT Senden ----------
void mqttPublishBattery() {

    // 1. Akku messen (Beispiel: ADC-Pin)
    uint8_t percentage;
    float vBat;
    vBat = getVbatt();
    percentage = mapFloat(vBat, minVoltage, maxVoltage);
    #ifdef DEBUG
    Serial.printf("Battery: %.2fV (%d%%)\n", vBat, percentage);
    #endif


    // 2. WLAN aktivieren
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(10);
    }

    // 3. MQTT verbinden
    client.setServer(mqtt_server, mqtt_port);
    if (client.connect("ESP32C6_Batt_Client")) {
        char payload[32];
        
        client.publish(mqtt_topic, payload);
        #ifdef DEBUG
        Serial.printf("MQTT gesendet: %s\n", payload);
        #endif
    }

    client.disconnect();

    // 4. WLAN ausschalten → wieder ESP-NOW only
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // 5. ESP-NOW wieder aktivieren
    setupEspNow();
}

float getVbatt() {
  uint32_t Vbatt = 0;
  for (int i = 0; i < 16; i++) {
    Vbatt += analogReadMilliVolts(A0);  // Read and accumulate ADC voltage
  }
  return (2 * Vbatt / 16 / 1000.0);  // Adjust for 1:2 divider and convert to volts
}

uint8_t mapFloat(float x, float in_min, float in_max) {
  float val;
  val = (x - in_min) * (100) / (in_max - in_min);
  if (val < 0) {
    val = 0;
  } else if (val > 100) {
    val = 100;
  }
  return (uint8_t)val;
}

void toggleLEDsFromDoorState(DoorState state){
  if(state == DoorState::OPEN){
    digitalWrite(D8, HIGH);
    digitalWrite(D1, HIGH);
    digitalWrite(D2, HIGH);
    digitalWrite(D7, HIGH);
  }
  else if(state == DoorState::CLOSED){
    digitalWrite(D8, LOW);
    digitalWrite(D1, LOW);
    digitalWrite(D2, LOW);
    digitalWrite(D7, LOW);
  }
}

void setup() {
  Serial.begin(115200);

  // WLAN im STA-Modus (Station) starten
  WiFi.mode(WIFI_STA);
  #ifdef DEBUG
  Serial.println("ESPNow Empfänger gestartet.");
  Serial.print("WiFi MAC: ");
  #endif
  Serial.println(WiFi.macAddress());

  // ESP-NOW initialisieren
  if (esp_now_init() != ESP_OK) {
    Serial.println("Fehler beim Start von ESP-NOW");
    return;
  }

  //Pins der LEDs als Output setzen
  pinMode(D8, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D7, OUTPUT);

  // Pin für Spannungsmessung als Input setzen
  pinMode(A0, INPUT);

  // Callback registrieren
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // keine Schleifenlogik nötig — alles läuft über Callback
}
