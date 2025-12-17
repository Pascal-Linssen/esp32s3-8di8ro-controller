#include <Arduino.h>

void setup() {
  // Test série très basique
  Serial.begin(115200);
  delay(3000);  // Délai plus long pour être sûr
  
  Serial.println("=== TEST SERIE ESP32-S3 ===");
  Serial.println("Si vous voyez ce message, la série fonctionne !");
}

void loop() {
  static int counter = 0;
  Serial.printf("Test %d - ESP32 fonctionne !\n", counter++);
  delay(2000);
  
  if (counter > 10) {
    Serial.println("=== FIN DU TEST SERIE ===");
    while(1) delay(1000); // Arrêt
  }
}