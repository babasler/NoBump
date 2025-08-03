#include <NewPing.h>
#include <stdbool.h>

#define TRIG_PIN 2
#define ECHO_PIN 0
#define LED_PIN_RED 22
#define LED_PIN_GREEN 23
#define CLOSED_THREASH 2
#define MAX_DISTANCE 200
#define OK 0
#define OPEN true
#define CLOSED false
#define OPEN_THREASH 2
#define ERR -1 

uint8_t turnOnLED(uint8_t pin);
uint8_t turnOffLED(uint8_t pin);
uint8_t handleNewDoorStatusIfNessecary(bool currentDoorStatus);
uint8_t switchLEDState(bool doorStatus);
bool isDoorOpen(uint32_t distance);


bool doorStatus = false;

NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);


void setup() {
  Serial.begin(9600);
  while(!Serial);
  Serial.println("Starting now...");
  pinMode(LED_PIN_RED, OUTPUT);
  pinMode(LED_PIN_GREEN, OUTPUT);
}

void loop() {
  delay(500);
  bool currentDoorStatus = CLOSED;
  uint32_t distance = sonar.ping_cm();
  currentDoorStatus = isDoorOpen(distance);
  handleNewDoorStatusIfNessecary(currentDoorStatus);     
}

bool isDoorOpen(uint32_t distance){
  if(distance <= OPEN_THREASH){
    return OPEN;
  }
  else{
    return CLOSED;
  }
}

uint8_t handleNewDoorStatusIfNessecary(bool currentDoorStatus){
  if(currentDoorStatus == doorStatus){
    return OK;
  }
  else if (currentDoorStatus != doorStatus){
    if(currentDoorStatus == OPEN){
    switchLEDState(OPEN);
    doorStatus = OPEN;
    delay(200);
    return OK;
  } else if(currentDoorStatus == CLOSED){
    switchLEDState(CLOSED);
    doorStatus = CLOSED;
    delay(200);
    return OK;
  }  
  else{
    return ERR;
  }
}
}

uint8_t turnOnLED(uint8_t pin){
  digitalWrite(pin,HIGH);
  return OK;
}

uint8_t turnOffLED(uint8_t pin){
  digitalWrite(pin,LOW);
  return OK;
}

uint8_t switchLEDState(bool doorStatus){
  if(doorStatus == OPEN){
    turnOnLED(LED_PIN_GREEN);
    turnOffLED(LED_PIN_RED);
    return OK;
  }
  else if(doorStatus == CLOSED){
    turnOnLED(LED_PIN_RED);
    turnOffLED(LED_PIN_GREEN);
    return OK;
  }
  else{
    return ERR;
  }
}


