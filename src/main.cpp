#include <Arduino.h>
#include <Ethernet.h>
#include <Wire.h>
#include <TCA9554.h>
#include <DHT.h>

// ===== CONFIGURATION PINS WAVESHARE OFFICIELS =====
// W5500 Ethernet
#define ETH_CS    16
#define ETH_RST   39
#define ETH_SCK   15
#define ETH_MISO  14
#define ETH_MOSI  13

// Entrées digitales (corrigées)
#define INPUT_1   4
#define INPUT_2   5
#define INPUT_3   6
#define INPUT_4   7
#define INPUT_5   8
#define INPUT_6   9
#define INPUT_7   10
#define INPUT_8   11

// DHT22 et I2C
#define DHT_PIN   12
#define DHT_TYPE  DHT22
#define SDA_PIN   42
#define SCL_PIN   41

// ===== CONFIGURATION RESEAU =====
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 50);
IPAddress dns_server(8, 8, 8, 8);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// ===== OBJETS GLOBAUX =====
TCA9554 tca(0x20);
DHT dht(DHT_PIN, DHT_TYPE);

// ===== VARIABLES GLOBALES =====
bool ethernetOK = false;
bool tcaOK = false;
bool inputs[8] = {false};
bool relays[8] = {false};
float temperature = 0.0;
float humidity = 0.0;
unsigned long lastSensorRead = 0;
unsigned long lastStatusPrint = 0;

// ===== FONCTIONS UTILITAIRES =====

void setupInputs() {
  Serial.println("Configuration des entrées digitales...");
  pinMode(INPUT_1, INPUT_PULLUP);
  pinMode(INPUT_2, INPUT_PULLUP);
  pinMode(INPUT_3, INPUT_PULLUP);
  pinMode(INPUT_4, INPUT_PULLUP);
  pinMode(INPUT_5, INPUT_PULLUP);
  pinMode(INPUT_6, INPUT_PULLUP);
  pinMode(INPUT_7, INPUT_PULLUP);
  pinMode(INPUT_8, INPUT_PULLUP);
  Serial.println("✓ Entrées configurées (pins 4-11)");
}

void setupI2C() {
  Serial.println("Configuration I2C pour TCA9554...");
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  delay(100);
  
  if (tca.begin()) {
    tcaOK = true;
    Serial.println("✓ TCA9554 détecté et initialisé");
    for (int i = 0; i < 8; i++) {
      tca.pinMode1(i, OUTPUT);
      tca.write1(i, LOW);
    }
  } else {
    tcaOK = false;
    Serial.println("✗ ERREUR: TCA9554 non détecté");
  }
}

void setupDHT() {
  Serial.println("Configuration DHT22...");
  dht.begin();
  delay(2000);
  float t = dht.readTemperature();
  if (!isnan(t)) {
    Serial.println("✓ DHT22 opérationnel");
  } else {
    Serial.println("✗ DHT22 non détecté");
  }
}

void setupEthernet() {
  Serial.println("Configuration Ethernet W5500...");
  pinMode(ETH_RST, OUTPUT);
  digitalWrite(ETH_RST, LOW);
  delay(100);
  digitalWrite(ETH_RST, HIGH);
  delay(500);
  
  Ethernet.init(ETH_CS);
  Ethernet.begin(mac, ip, dns_server, gateway, subnet);
  delay(2000);
  
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("✗ ERREUR: W5500 non détecté");
    ethernetOK = false;
  } else if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("✗ Câble Ethernet non connecté");
    ethernetOK = false;
  } else {
    ethernetOK = true;
    Serial.print("✓ Ethernet connecté - IP: ");
    Serial.println(Ethernet.localIP());
  }
}

void printHelp() {
  Serial.println("=== COMMANDES DISPONIBLES ===");
  Serial.println("help        - Affiche cette aide");
  Serial.println("status      - Etat du système");
  Serial.println("scan        - Scan des périphériques I2C");
  Serial.println("testio      - Test des entrées/sorties");
  Serial.println("pins        - Informations sur les pins");
  Serial.println("testpins    - Test différentes combinaisons I2C");
  Serial.println("relay X on  - Active le relais X (1-8)");
  Serial.println("relay X off - Désactive le relais X (1-8)");
  Serial.println("=============================");
}

void printStatus() {
  Serial.println("=== ETAT DU SYSTEME ===");
  Serial.print("Ethernet W5500: ");
  if (ethernetOK) {
    Serial.print("✓ OK - IP: ");
    Serial.println(Ethernet.localIP());
  } else {
    Serial.println("✗ ERREUR");
  }
  
  Serial.print("TCA9554 I2C: ");
  Serial.println(tcaOK ? "✓ OK" : "✗ ERREUR");
  
  Serial.print("Température: ");
  Serial.print(temperature);
  Serial.print("°C, Humidité: ");
  Serial.print(humidity);
  Serial.println("%");
  
  Serial.print("Entrées: ");
  for (int i = 0; i < 8; i++) {
    Serial.print(i + 1);
    Serial.print(":");
    Serial.print(inputs[i] ? "1" : "0");
    Serial.print(" ");
  }
  Serial.println();
  
  Serial.print("Relais: ");
  for (int i = 0; i < 8; i++) {
    Serial.print(i + 1);
    Serial.print(":");
    Serial.print(relays[i] ? "ON" : "OFF");
    Serial.print(" ");
  }
  Serial.println();
  Serial.println("======================");
}

void scanI2C() {
  Serial.println("=== SCAN I2C ===");
  Serial.print("SDA: Pin ");
  Serial.print(SDA_PIN);
  Serial.print(", SCL: Pin ");
  Serial.println(SCL_PIN);
  
  int devices = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("Périphérique trouvé à l'adresse 0x");
      Serial.print(addr, HEX);
      Serial.print(" (");
      Serial.print(addr);
      Serial.println(")");
      devices++;
    }
  }
  
  if (devices == 0) {
    Serial.println("Aucun périphérique I2C trouvé");
  } else {
    Serial.print("Total: ");
    Serial.print(devices);
    Serial.println(" périphérique(s) trouvé(s)");
  }
  Serial.println("================");
}

void testI2CPins() {
  Serial.println("=== TEST COMBINAISONS I2C ===");
  // Pins disponibles pour I2C (pins officiels Waveshare en premier)
  int pinCombos[][2] = {
    {42, 41}, // Pins officiels Waveshare !! 
    {1, 2},   // Ancien test
    {17, 18}, // Alternative 1
    {19, 20}, // Alternative 2  
    {35, 36}, // Alternative 3
    {37, 38}, // Alternative 4
    {45, 46}, // Alternative 5
    {47, 48}  // Alternative 6
  };
  
  int numCombos = sizeof(pinCombos) / sizeof(pinCombos[0]);
  
  for (int i = 0; i < numCombos; i++) {
    int sda = pinCombos[i][0];
    int scl = pinCombos[i][1];
    
    Serial.print("Test SDA=");
    Serial.print(sda);
    Serial.print(", SCL=");
    Serial.print(scl);
    Serial.print(" : ");
    
    Wire.end();
    delay(100);
    Wire.begin(sda, scl);
    Wire.setClock(100000);
    delay(100);
    
    Wire.beginTransmission(0x20);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.println("✓ TCA9554 détecté !");
    } else {
      Serial.println("✗ Pas de réponse");
    }
  }
  
  Wire.end();
  delay(100);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  Serial.println("=============================");
}

void testIO() {
  Serial.println("=== TEST ENTREES/SORTIES ===");
  Serial.println("Entrées actuelles:");
  for (int i = 0; i < 8; i++) {
    int pin = INPUT_1 + i;
    bool state = !digitalRead(pin);
    Serial.print("  IN");
    Serial.print(i + 1);
    Serial.print(" (pin ");
    Serial.print(pin);
    Serial.print("): ");
    Serial.println(state ? "ACTIVE" : "INACTIVE");
  }
  
  if (tcaOK) {
    Serial.println("Test séquentiel des relais:");
    for (int i = 0; i < 8; i++) {
      Serial.print("  Relais ");
      Serial.print(i + 1);
      Serial.print(" ON... ");
      
      if (tca.write1(i, HIGH) == TCA9554_OK) {
        Serial.println("✓");
        relays[i] = true;
        delay(500);
        tca.write1(i, LOW);
        relays[i] = false;
      } else {
        Serial.println("✗ ERREUR");
      }
      delay(200);
    }
  } else {
    Serial.println("TCA9554 non disponible - pas de test relais");
  }
  Serial.println("============================");
}

void printPinsInfo() {
  Serial.println("=== CONFIGURATION PINS ===");
  Serial.println("Ethernet W5500:");
  Serial.print("  CS: "); Serial.println(ETH_CS);
  Serial.print("  RST: "); Serial.println(ETH_RST);
  Serial.print("  SCK: "); Serial.println(ETH_SCK);
  Serial.print("  MISO: "); Serial.println(ETH_MISO);
  Serial.print("  MOSI: "); Serial.println(ETH_MOSI);
  
  Serial.println("Entrées digitales:");
  for (int i = 0; i < 8; i++) {
    Serial.print("  IN"); Serial.print(i + 1);
    Serial.print(": Pin "); Serial.println(INPUT_1 + i);
  }
  
  Serial.println("I2C TCA9554:");
  Serial.print("  SDA: "); Serial.println(SDA_PIN);
  Serial.print("  SCL: "); Serial.println(SCL_PIN);
  
  Serial.println("DHT22:");
  Serial.print("  Data: "); Serial.println(DHT_PIN);
  Serial.println("===========================");
}

void handleRelayCommand(String command) {
  int firstSpace = command.indexOf(' ');
  int secondSpace = command.indexOf(' ', firstSpace + 1);
  
  if (firstSpace == -1 || secondSpace == -1) {
    Serial.println("Format: relay X on/off (X=1-8)");
    return;
  }
  
  int relayNum = command.substring(firstSpace + 1, secondSpace).toInt();
  String action = command.substring(secondSpace + 1);
  
  if (relayNum < 1 || relayNum > 8) {
    Serial.println("Numéro de relais invalide (1-8)");
    return;
  }
  
  if (!tcaOK) {
    Serial.println("Erreur: TCA9554 non disponible");
    return;
  }
  
  int relayIndex = relayNum - 1;
  bool newState = (action == "on");
  
  if (tca.write1(relayIndex, newState) == TCA9554_OK) {
    relays[relayIndex] = newState;
    Serial.print("Relais ");
    Serial.print(relayNum);
    Serial.print(" ");
    Serial.println(newState ? "activé" : "désactivé");
  } else {
    Serial.print("Erreur activation relais ");
    Serial.println(relayNum);
  }
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    if (command == "help") {
      printHelp();
    } else if (command == "status") {
      printStatus();
    } else if (command == "scan") {
      scanI2C();
    } else if (command == "testio") {
      testIO();
    } else if (command == "pins") {
      printPinsInfo();
    } else if (command.startsWith("relay")) {
      handleRelayCommand(command);
    } else if (command == "testpins") {
      testI2CPins();
    } else {
      Serial.println("Commande inconnue. Tapez 'help' pour l'aide.");
    }
  }
}

void readInputs() {
  inputs[0] = !digitalRead(INPUT_1);
  inputs[1] = !digitalRead(INPUT_2);
  inputs[2] = !digitalRead(INPUT_3);
  inputs[3] = !digitalRead(INPUT_4);
  inputs[4] = !digitalRead(INPUT_5);
  inputs[5] = !digitalRead(INPUT_6);
  inputs[6] = !digitalRead(INPUT_7);
  inputs[7] = !digitalRead(INPUT_8);
}

void readSensors() {
  float newTemp = dht.readTemperature();
  float newHum = dht.readHumidity();
  
  if (!isnan(newTemp)) temperature = newTemp;
  if (!isnan(newHum)) humidity = newHum;
}

void setup() {
  Serial.begin(9600);
  delay(3000);
  
  Serial.println("=== ESP32-S3-ETH-8DI-8RO DEMARRAGE ===");
  Serial.println("Configuration pins Waveshare officiels");
  
  setupInputs();
  setupI2C();
  setupDHT();
  setupEthernet();
  
  Serial.println("=== SYSTEME PRET ===");
  Serial.println("Commandes disponibles:");
  Serial.println("- 'help' : Aide complète");
  Serial.println("- 'status' : Etat du système");
  Serial.println("- 'scan' : Scan I2C");
  Serial.println("- 'testpins' : Test pins I2C");
  Serial.println("========================");
}

void loop() {
  handleSerialCommands();
  readInputs();
  
  if (millis() - lastSensorRead > 5000) {
    readSensors();
    lastSensorRead = millis();
  }
  
  if (millis() - lastStatusPrint > 30000) {
    printStatus();
    lastStatusPrint = millis();
  }
  
  delay(100);
}