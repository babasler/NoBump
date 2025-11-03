#include <NewPing.h>
#include <stdbool.h>
#include <WiFi.h>
#include <esp_now.h>

#define TRIG_PIN 0
#define ECHO_PIN 1
#define MAX_DISTANCE 200
#define DOOR_CLOSED_THREASH 4


NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);
// MAC-Adresse des Empfängers (anpassen!)
uint8_t receiverMac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

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

DoorState getStateFromDistance(uint32_t distance);

void setup() {
  Serial.begin(115200);

  // WLAN im Station-Modus starten (ohne Verbindung)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();  // Keine Verbindung zu Access Point nötig
  delay(100);

  // ESP-NOW initialisieren
  if (esp_now_init() != ESP_OK) {
    Serial.println("Fehler beim Initialisieren von ESP-NOW!");
    return;
  }
  Serial.println("ESP-NOW erfolgreich initialisiert.");

  // Callback registrieren
  esp_now_register_send_cb(OnDataSent);

  // Peer (Empfänger) hinzufügen
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMac, 6);
  peerInfo.channel = 0;  // gleicher Kanal wie WiFi
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Fehler beim Hinzufügen des Peers!");
    return;
  }
}

void loop() {
  //Distanz messen
  uint32_t distance = sonar.ping_cm();

  //auf Doorstate prüfen und in Nachricht verpacken
  messaagetStateFromDistance(distance);

  // Daten senden
  esp_err_t result = esp_now_send(receiverMac, (uint8_t *)&message, sizeof(message));

  if (result == ESP_OK) {
    Serial.println("Daten gesendet");
  } else {
    Serial.println("Fehler beim Senden!");
  }

  delay(2000);  // alle 2 Sekunden senden
}

DoorState getStateFromDistance(uint32_t distance){
  if(distance > DOOR_CLOSED_THREASH){
    return DoorState:OPEN;
  }
  else if(distance <= DOOR_CLOSED_THREASH){
    return DoorState:CLOSED
  }
  else{
    return DoorState:ERROR
  }
}

// Callback, wenn Nachricht gesendet wurde
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Senden an: ");
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2],
           mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
  Serial.print(" --> Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Erfolgreich" : "Fehlgeschlagen");
}


