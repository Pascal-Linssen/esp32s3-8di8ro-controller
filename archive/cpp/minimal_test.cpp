#include <Arduino.h>

void setup() {
  Serial.begin(9600);
  delay(3000);
  Serial.println("=== ESP32 MINIMAL TEST ===");
  Serial.println("ESP32 fonctionne!");
}

void loop() {
  Serial.println("Loop fonctionne - " + String(millis()));
  delay(2000);
}