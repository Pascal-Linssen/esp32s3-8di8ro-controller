// Test simple pour déboguer W5500 - examine le port série du W5500
#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>

#define ETH_SCK_PIN  15
#define ETH_MISO_PIN 14
#define ETH_MOSI_PIN 13
#define ETH_CS_PIN   16
#define ETH_RST_PIN  39
#define ETH_IRQ_PIN  12

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress staticIP(192, 168, 1, 50);

void setup() {
  Serial.begin(9600);
  delay(3000);
  Serial.println("\n=== W5500 SPI Detection Test ===");
  
  // SPI
  SPI.begin(ETH_SCK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, ETH_CS_PIN);
  pinMode(ETH_CS_PIN, OUTPUT);
  pinMode(ETH_RST_PIN, OUTPUT);
  pinMode(ETH_IRQ_PIN, INPUT);
  
  digitalWrite(ETH_CS_PIN, HIGH);
  
  // Reset
  digitalWrite(ETH_RST_PIN, LOW);
  delay(100);
  digitalWrite(ETH_RST_PIN, HIGH);
  delay(1000);
  
  Serial.println("✓ Reset terminé");
  
  // Test: Read W5500 version register (0x0039)
  // This should be 0x04 for W5500
  digitalWrite(ETH_CS_PIN, LOW);
  SPI.transfer(0x00);  // Read operation
  SPI.transfer(0x39);  // Address high byte
  SPI.transfer(0x00);  // Address low byte
  byte version = SPI.transfer(0x00);
  digitalWrite(ETH_CS_PIN, HIGH);
  
  Serial.printf("W5500 Version Register: 0x%02X (expect 0x04)\n", version);
  
  if (version == 0x04) {
    Serial.println("✓ W5500 detected correctly!");
  } else {
    Serial.println("✗ W5500 not detected or SPI error");
  }
}

void loop() {
  delay(1000);
}
