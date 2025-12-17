#include <Arduino.h>
#include <Ethernet.h>
#include <Wire.h>
#include <DHT.h>
#include <SPI.h>

// Configuration réseau
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress staticIP(192, 168, 1, 50);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns1(8, 8, 8, 8);

// Pins SPI W5500 - VRAIE CONFIG WAVESHARE selon WS_ETH.h
#define ETH_CS_PIN    16    // CS = Pin 16 (selon Waveshare)
#define ETH_RST_PIN   39    // Reset = Pin 39 (selon Waveshare)
#define ETH_IRQ_PIN   12    // IRQ = Pin 12 (selon Waveshare)
#define ETH_SCK_PIN   15    // SCK = Pin 15 (selon Waveshare)
#define ETH_MISO_PIN  14    // MISO = Pin 14 (selon Waveshare)
#define ETH_MOSI_PIN  13    // MOSI = Pin 13 (selon Waveshare)

// Configuration matérielle
#define DHT_PIN 40
#define DHT_TYPE DHT22
#define TCA9554_ADDR 0x20

// Pins I2C pour TCA9554 (test pins par défaut)
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// Pins des entrées digitales (pins réels selon schéma Waveshare)
const int digitalInputs[8] = {4, 5, 6, 7, 8, 9, 10, 11};

DHT dht(DHT_PIN, DHT_TYPE);

// États des relais et entrées
bool relayStates[8] = {false};
bool inputStates[8] = {false};
float temperature = 0.0;
float humidity = 0.0;

// Fonctions TCA9554 pour contrôle des relais
void tca9554_write(uint8_t data) {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(0x01); // Output register
  Wire.write(~data); // Inversion car les relais sont actifs à LOW
  Wire.endTransmission();
  
  // Debug
  Serial.printf("TCA9554 écrit: 0x%02X (relais data: 0x%02X)\n", ~data, data);
}

void tca9554_init() {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(0x03); // Configuration register
  Wire.write(0x00); // Tous en sortie
  uint8_t result = Wire.endTransmission();
  
  if (result == 0) {
    Serial.println("✓ TCA9554 communication OK");
  } else {
    Serial.printf("❌ TCA9554 erreur communication: %d\n", result);
  }
  
  // Tous les relais OFF au démarrage
  tca9554_write(0x00);
}

void setRelay(int relay, bool state) {
  if (relay >= 0 && relay < 8) {
    relayStates[relay] = state;
    uint8_t relayData = 0;
    for (int i = 0; i < 8; i++) {
      if (relayStates[i]) {
        relayData |= (1 << i);
      }
    }
    tca9554_write(relayData);
  }
}

void readInputs() {
  for (int i = 0; i < 8; i++) {
    inputStates[i] = digitalRead(digitalInputs[i]);
  }
}

void readSensors() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  
  if (isnan(temperature)) temperature = 0.0;
  if (isnan(humidity)) humidity = 0.0;
}

// Fonction de scan I2C pour diagnostiquer
void scanI2C() {
  Serial.println("\n=== SCAN I2C ===");
  byte error, address;
  int nDevices = 0;
  
  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.printf("Device I2C trouvé à l'adresse 0x%02X\n", address);
      nDevices++;
    }
  }
  
  if (nDevices == 0) {
    Serial.println("❌ Aucun device I2C trouvé");
  } else {
    Serial.printf("✓ %d device(s) I2C trouvé(s)\n", nDevices);
  }
  Serial.println("================");
}

// Test direct du TCA9554
void testTCA9554() {
  Serial.println("\n=== TEST TCA9554 ===");
  
  // Test écriture/lecture
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(0x01); // Output register
  Wire.write(0xFF); // Tous HIGH
  uint8_t result = Wire.endTransmission();
  
  Serial.printf("Écriture 0xFF: %s\n", result == 0 ? "OK" : "ERREUR");
  
  delay(500);
  
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(0x01); // Output register  
  Wire.write(0x00); // Tous LOW
  result = Wire.endTransmission();
  
  Serial.printf("Écriture 0x00: %s\n", result == 0 ? "OK" : "ERREUR");
  Serial.println("==================");
}

void setup() {
  // Initialisation série avec délai pour démarrage
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n=== DÉMARRAGE ESP32-S3-ETH-8DI-8RO ===");
  Serial.println("Version: Test I2C Pins");
  Serial.println("Date: " + String(__DATE__) + " " + String(__TIME__));
  
  // Test de base
  for (int i = 0; i < 5; i++) {
    Serial.printf("Test %d/5\n", i+1);
    delay(500);
  }
  
  // Initialisation I2C avec pins spécifiques Waveshare
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.printf("✓ I2C initialisé (SDA=%d, SCL=%d)\n", I2C_SDA_PIN, I2C_SCL_PIN);
  
  // Initialisation TCA9554
  tca9554_init();
  Serial.println("✓ TCA9554 initialisé (relais)");
  
  // Initialisation DHT22
  dht.begin();
  Serial.println("✓ DHT22 initialisé");
  
  // Configuration des entrées digitales
  for (int i = 0; i < 8; i++) {
    pinMode(digitalInputs[i], INPUT_PULLUP);
  }
  Serial.println("✓ Entrées digitales configurées");
  
  // Initialisation SPI avec les VRAIS pins Waveshare
  Serial.println("Configuration SPI avec pins Waveshare...");
  SPI.begin(ETH_SCK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, ETH_CS_PIN);
  Serial.printf("✓ SPI: SCK=%d, MISO=%d, MOSI=%d, CS=%d\n", 
                ETH_SCK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, ETH_CS_PIN);
  
  // Configuration des pins
  pinMode(ETH_CS_PIN, OUTPUT);
  pinMode(ETH_RST_PIN, OUTPUT);
  pinMode(ETH_IRQ_PIN, INPUT);
  
  digitalWrite(ETH_CS_PIN, HIGH);  // CS inactif
  
  // Reset du W5500 avec les vrais pins
  Serial.printf("Reset W5500 (pin %d)...\n", ETH_RST_PIN);
  digitalWrite(ETH_RST_PIN, LOW);
  delay(100);
  digitalWrite(ETH_RST_PIN, HIGH);
  delay(1000);
  
  Serial.println("✓ Reset W5500 terminé");
  
  // Initialisation Ethernet
  Ethernet.init(ETH_CS_PIN);
  Serial.print("Initialisation Ethernet...");
  
  Ethernet.begin(mac, staticIP, dns1, gateway, subnet);
  delay(2000);
  
  // Vérification
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println(" ✗");
    Serial.println("❌ W5500 non détecté avec ces pins!");
    Serial.printf("Pins testés: CS=%d, RST=%d, IRQ=%d\n", 
                  ETH_CS_PIN, ETH_RST_PIN, ETH_IRQ_PIN);
    Serial.printf("SPI: SCK=%d, MISO=%d, MOSI=%d\n", 
                  ETH_SCK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN);
  } else if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println(" ⚠️");
    Serial.println("⚠️  W5500 détecté mais pas de câble");
    Serial.println("Connectez un câble Ethernet");
  } else {
    Serial.println(" ✓");
    Serial.print("✓ IP Ethernet: ");
    Serial.println(Ethernet.localIP());
    Serial.print("✓ Interface web: http://");
    Serial.println(Ethernet.localIP());
  }
  
  Serial.println("\n=== Système prêt ===");
  Serial.println("Tapez 'help' pour les commandes disponibles");
}

void loop() {
  Ethernet.maintain();
  
  // Lecture des capteurs et entrées toutes les 2 secondes
  static uint32_t lastSensorRead = 0;
  if (millis() - lastSensorRead > 2000) {
    lastSensorRead = millis();
    readSensors();
    readInputs();
  }
  
  // Commandes série pour debug et contrôle immédiat
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.startsWith("relay ")) {
      int relayNum = command.substring(6, 7).toInt();
      String state = command.substring(8);
      
      if (relayNum >= 1 && relayNum <= 8) {
        bool newState = (state == "on" || state == "ON");
        setRelay(relayNum - 1, newState);
        Serial.printf("Relais %d: %s\n", relayNum, newState ? "ON" : "OFF");
      }
    }
    else if (command == "status") {
      Serial.println("\n=== STATUS DETAILLE ===");
      Serial.printf("Hardware: %d ", Ethernet.hardwareStatus());
      switch(Ethernet.hardwareStatus()) {
        case EthernetNoHardware: Serial.println("(Pas de W5500)"); break;
        case EthernetW5100: Serial.println("(W5100 détecté)"); break;
        case EthernetW5200: Serial.println("(W5200 détecté)"); break;
        case EthernetW5500: Serial.println("(W5500 détecté ✓)"); break;
        default: Serial.println("(Inconnu)"); break;
      }
      
      Serial.printf("Link: %d ", Ethernet.linkStatus());
      switch(Ethernet.linkStatus()) {
        case Unknown: Serial.println("(Indéterminé)"); break;
        case LinkON: Serial.println("(Câble connecté ✓)"); break;
        case LinkOFF: Serial.println("(Pas de câble)"); break;
        default: Serial.println("(Inconnu)"); break;
      }
      
      if (Ethernet.hardwareStatus() == EthernetW5500) {
        Serial.print("IP: ");
        Serial.println(Ethernet.localIP());
        Serial.print("Interface web: http://");
        Serial.println(Ethernet.localIP());
      }
      
      Serial.printf("Température: %.1f°C, Humidité: %.1f%%\n", temperature, humidity);
      
      // Affichage de l'état des relais
      Serial.println("\n--- État des Relais ---");
      for (int i = 0; i < 8; i++) {
        Serial.printf("Relais %d: %s\n", i+1, relayStates[i] ? "ON" : "OFF");
      }
      
      // Affichage de l'état des entrées
      Serial.println("\n--- État des Entrées ---");
      for (int i = 0; i < 8; i++) {
        Serial.printf("Entrée %d: %s\n", i+1, inputStates[i] ? "HIGH" : "LOW");
      }
      
      Serial.println("======================");
    }
    else if (command == "inputs") {
      Serial.println("\n=== ENTREES DIGITALES ===");
      for (int i = 0; i < 8; i++) {
        Serial.printf("Entrée %d (Pin %d): %s\n", i+1, digitalInputs[i], inputStates[i] ? "HIGH ⚡" : "LOW ⏸️");
      }
      Serial.println("========================");
    }
    else if (command == "relays") {
      Serial.println("\n=== ETAT DES RELAIS ===");
      for (int i = 0; i < 8; i++) {
        Serial.printf("Relais %d: %s\n", i+1, relayStates[i] ? "ON 🟢" : "OFF 🔴");
      }
      Serial.println("=====================");
    }
    else if (command == "test") {
      Serial.println("\n=== TEST SEQUENCE ===");
      Serial.println("Test des relais 1-8...");
      for (int i = 0; i < 8; i++) {
        Serial.printf("Activation relais %d...\n", i+1);
        setRelay(i, true);
        delay(500);
        Serial.printf("Désactivation relais %d...\n", i+1);
        setRelay(i, false);
        delay(300);
      }
      Serial.println("Test terminé !");
    }
    else if (command.startsWith("all ")) {
      String state = command.substring(4);
      bool newState = (state == "on" || state == "ON");
      Serial.printf("Tous les relais: %s\n", newState ? "ON" : "OFF");
      for (int i = 0; i < 8; i++) {
        setRelay(i, newState);
      }
    }
    else if (command == "scan") {
      scanI2C();
    }
    else if (command == "testpins") {
      Serial.println("\n=== TEST PINS I2C ===");
      // Combinaisons courantes pour ESP32-S3
      int pinCombos[][2] = {
        {21, 22}, // Défaut ESP32-S3
        {41, 42}, // Waveshare possible
        {47, 48}, // Alternative ESP32-S3
        {1, 2},   // Alternative
        {3, 4},   // Alternative
        {5, 6},   // Alternative
        {17, 18}, // Alternative
        {19, 20}  // Alternative
      };
      
      for (int i = 0; i < 8; i++) {
        Serial.printf("Test SDA=%d, SCL=%d: ", pinCombos[i][0], pinCombos[i][1]);
        Wire.end();
        Wire.begin(pinCombos[i][0], pinCombos[i][1]);
        Wire.setClock(100000);
        
        Wire.beginTransmission(TCA9554_ADDR);
        byte error = Wire.endTransmission();
        if (error == 0) {
          Serial.println("✓ TCA9554 TROUVÉ!");
          break;
        } else {
          Serial.println("✗");
        }
        delay(100);
      }
      
      // Retour aux pins configurés
      Wire.end();
      Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
      Serial.println("======================");
    }
    
    else if (command == "testio") {
      testTCA9554();
    }
    else if (command == "pins") {
      Serial.println("\n=== CONFIGURATION PINS ===");
      Serial.println("📥 Entrées digitales:");
      for (int i = 0; i < 8; i++) {
        Serial.printf("  Entrée %d -> Pin %d\n", i+1, digitalInputs[i]);
      }
      Serial.println("🔌 Relais:");
      Serial.printf("  TCA9554 I2C @ 0x%02X\n", TCA9554_ADDR);
      Serial.printf("  I2C: SDA=%d, SCL=%d\n", I2C_SDA_PIN, I2C_SCL_PIN);
      Serial.println("🌡️ Capteur:");
      Serial.printf("  DHT22 -> Pin %d\n", DHT_PIN);
      Serial.println("🌐 Ethernet W5500:");
      Serial.printf("  CS=%d, RST=%d, IRQ=%d\n", ETH_CS_PIN, ETH_RST_PIN, ETH_IRQ_PIN);
      Serial.printf("  SPI: SCK=%d, MISO=%d, MOSI=%d\n", ETH_SCK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN);
      Serial.println("============================");
    }
    else if (command == "help") {
      Serial.println("\n=== COMMANDES DISPONIBLES ===");
      Serial.println("📋 Informations:");
      Serial.println("  help           - Cette aide");
      Serial.println("  status         - Status détaillé complet");
      Serial.println("  inputs         - État des 8 entrées digitales");
      Serial.println("  relays         - État des 8 relais");
      Serial.println("  pins           - Configuration des pins");
      Serial.println("");
      Serial.println("🔌 Contrôle des relais:");
      Serial.println("  relay X on/off - Contrôler relais 1-8");
      Serial.println("  all on/off     - Tous les relais ON/OFF");
      Serial.println("  test           - Test séquentiel de tous les relais");
      Serial.println("");
      Serial.println("� Diagnostic:");
      Serial.println("  scan           - Scan I2C des devices");
      Serial.println("  testio         - Test direct TCA9554");
      Serial.println("");
      Serial.println("�💡 Exemples:");
      Serial.println("  relay 1 on     - Active le relais 1");
      Serial.println("  relay 3 off    - Désactive le relais 3");
      Serial.println("  all on         - Active tous les relais");
      Serial.println("  scan           - Cherche devices I2C");
      Serial.println("=============================");
    }
  }
  
  // Status périodique toutes les 30s
  static uint32_t lastStatus = 0;
  if (millis() - lastStatus > 30000) {
    lastStatus = millis();
    
    if (Ethernet.hardwareStatus() == EthernetW5500) {
      if (Ethernet.linkStatus() == LinkON) {
        Serial.print("✓ Ethernet OK - Interface: http://");
        Serial.println(Ethernet.localIP());
      } else {
        Serial.println("⚠️  W5500 OK mais pas de câble Ethernet");
      }
    } else {
      Serial.println("❌ W5500 non détecté - vérifiez les connexions");
    }
  }
}