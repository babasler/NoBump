#include <NewPing.h>
#include <stdbool.h>
#include <WiFi.h>
#include <esp_now.h>

#define TRIG_PIN 19
#define ECHO_PIN 17
#define MAX_DISTANCE 200
#define DOOR_CLOSED_THREASH 4


NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);
// MAC-Adresse des Empfängers (anpassen!)
uint8_t receiverMac[] = {0x10, 0x51, 0xDB, 0x1A, 0xE6, 0xC8};

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

// aktueller zustand
DoorState currentState;

void setup() {
  Serial.begin(115200);
  Serial.println("Sender gestartet");
  
  // WLAN im Station-Modus starten (ohne Verbindung)
  WiFi.mode(WIFI_STA);

  Serial.print("WiFi MAC: ");
  Serial.println(WiFi.macAddress());

  WiFi.disconnect();  // Keine Verbindung zu Access Point nötig
  delay(100);

  // ESP-NOW initialisieren
  if (esp_now_init() != ESP_OK) {
    Serial.println("Fehler beim Initialisieren von ESP-NOW!");
    return;
  }
  Serial.println("ESP-NOW erfolgreich initialisiert.");

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

  if(distance == 0){
    Serial.println("Fehler bei der Distanzmessung");
    delay(1000);
    return;
  }

  //nur wenn sich der zustand geändert hat, nachricht verpacken und senden
  DoorState newState = getStateFromDistance(distance);
  if(newState == currentState){
    Serial.println("Zustand unverändert, sende keine Nachricht.");
    delay(1000);
    return;
  }

  currentState = newState;

  //auf Doorstate prüfen und in Nachricht verpacken
  message.state = currentState;

  Serial.print("Gemessene Distanz: "); Serial.print(distance); Serial.print(" cm, neuer Zustand: ");
  Serial.println(currentState == DoorState::OPEN ? "OPEN" : (currentState == DoorState::CLOSED ? "CLOSED" : "ERROR"));
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
    return DoorState::OPEN;
  }
  else if(distance <= DOOR_CLOSED_THREASH){
    return DoorState::CLOSED;
  }
  else{
    return DoorState::OPEN; //default fallback
  }
}



