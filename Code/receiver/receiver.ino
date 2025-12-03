//Pins: GPIO 0,1,2,17
// Bei LED: Langes Bein (+) geht an ESP, kurzes Bein (-) an GND 
// ESP => 220 Ohm => Langes Bein (+) LED Kurzes Bein(-) => GND
#include <esp_now.h>
#include <WiFi.h>

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


// Callback, der ausgelöst wird, wenn Daten empfangen werden
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len){
  memcpy(&message, data, sizeof(message));
  uint8_t percentage;
  float vBat;

  Serial.print("Daten empfangen von: ");
  Serial.print("Sender MAC: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", info->src_addr[i]);
        if (i < 5) Serial.print(":");
    }

  Serial.print("Doorstate: "); Serial.println(message.state == DoorState::OPEN ? "OPEN" : (message.state == DoorState::CLOSED ? "CLOSED" : "ERROR"));

  toggleLEDsFromDoorState(message.state);

  vBat = getVbatt();
  percentage = mapFloat(vBat, minVoltage, maxVoltage);
  Serial.printf("Battery: %.2fV (%d%%)\n", vBat, percentage);
  //hier dann mqtt publishen
  
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
  Serial.println("ESPNow Empfänger gestartet.");
  Serial.print("WiFi MAC: ");
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

  // Configure A0 as ADC input for reading battery voltage
  pinMode(A0, INPUT);

  // Callback registrieren
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // keine Schleifenlogik nötig — alles läuft über Callback
}
