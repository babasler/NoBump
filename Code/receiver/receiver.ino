//Pins: GPIO 0,1,2,17
// Bei LED: Langes Bein (+) geht an ESP, kurzes Bein (-) an GND 
// ESP => 220 Ohm => Langes Bein (+) LED Kurzes Bein(-) => GND
#include <esp_now.h>
#include <WiFi.h>

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

// Callback, der ausgelöst wird, wenn Daten empfangen werden
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingDataBuffer, int len) {
  memcpy(&message, incomingDataBuffer, sizeof(message));

  Serial.print("Daten empfangen von: ");
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println(macStr);

  Serial.print("ID: "); Serial.println(incomingData.id);
  Serial.print("Temperatur: "); Serial.println(incomingData.temperature);
  Serial.print("Feuchtigkeit: "); Serial.println(incomingData.humidity);
  Serial.println("-------------------------");
}

void setup() {
  Serial.begin(115200);

  // WLAN im STA-Modus (Station) starten
  WiFi.mode(WIFI_STA);
  Serial.println("ESPNow Empfänger gestartet.");

  // ESP-NOW initialisieren
  if (esp_now_init() != ESP_OK) {
    Serial.println("Fehler beim Start von ESP-NOW");
    return;
  }

  // Callback registrieren
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // keine Schleifenlogik nötig — alles läuft über Callback
}
