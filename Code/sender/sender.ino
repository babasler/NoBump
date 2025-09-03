#include <NewPing.h>
#include <stdbool.h>

#define TRIG_PIN 0
#define ECHO_PIN 1
#define MAX_DISTANCE 200


NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);


void setup() {
  Serial.begin(9600);
  while(!Serial);
  Serial.println("Starting now...");
}

void loop() {
  delay(500);
  uint32_t distance = sonar.ping_cm();   
  printf("distance %d \n", distance);
}
