#include <Arduino.h>
#include <Ethernet.h>
#include <Wire.h>
#include <DHT.h>
#include <SPI.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Configuration réseau
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress staticIP(192, 168, 1, 50);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns1(8, 8, 8, 8);

// Configuration MQTT
IPAddress mqttServer(192, 168, 1, 200); // Home Assistant Mosquitto broker
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "ESP32S3_8DI8RO"
#define MQTT_USER "pascal"    // Login MQTT
#define MQTT_PASS "123456"    // Password MQTT

// Topics MQTT
#define TOPIC_STATUS "esp32s3/status"
#define TOPIC_RELAY_CMD "esp32s3/relay/cmd"
#define TOPIC_RELAY_STATE "esp32s3/relay/state"
#define TOPIC_INPUT_STATE "esp32s3/input/state"
#define TOPIC_SENSOR "esp32s3/sensor"

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

// Pins I2C pour TCA9554 - PINS WAVESHARE OFFICIELS
#define I2C_SDA_PIN 42
#define I2C_SCL_PIN 41

// Pins des entrées digitales (pins réels selon schéma Waveshare)
const int digitalInputs[8] = {4, 5, 6, 7, 8, 9, 10, 11};

DHT dht(DHT_PIN, DHT_TYPE);

// Clients réseau
EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

// États des relais et entrées
bool relayStates[8] = {false};
bool inputStates[8] = {false};
bool lastInputStates[8] = {false};
float temperature = 0.0;
float humidity = 0.0;
uint32_t lastMqttPublish = 0;
uint32_t lastMqttReconnect = 0;

// ===== FONCTIONS UTILITAIRES =====
void tca9554_write(uint8_t data) {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(0x01); // Output register
  Wire.write(data); // ENLEVER L'INVERSION - Test direct
  Wire.endTransmission();
  
  // Debug
  Serial.printf("TCA9554 écrit: 0x%02X (relais data: 0x%02X)\n", data, data);
}

void tca9554_init() {
  // IMPORTANT: Configurer tous les pins en sortie AVANT d'écrire les données
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(0x03); // Configuration register
  Wire.write(0x00); // Tous en sortie
  uint8_t result = Wire.endTransmission();
  
  if (result == 0) {
    Serial.println("✓ TCA9554 communication OK");
    // MAINTENANT ON SAIT: 0x00 = relais OFF, 0xFF = relais ON
    delay(50); // Pause pour stabilité
    
    Serial.println("Arrêt définitif de tous les relais...");
    tca9554_write(0x00); // TOUS OFF avec la bonne valeur
    delay(100);
    
    Serial.println("✓ Tous les relais forcés OFF au démarrage");
  } else {
    Serial.printf("❌ TCA9554 erreur communication: %d\n", result);
  }
}

void setRelay(int relay, bool state) {
  if (relay >= 0 && relay < 8) {
    relayStates[relay] = state;
    uint8_t relayData = 0;
    for (int i = 0; i < 8; i++) {
      if (relayStates[i]) {
        relayData |= (1 << i);  // Bit à 1 = relais ON
      }
    }
    tca9554_write(relayData);
    Serial.printf("Relais %d: %s (données: 0x%02X)\n", relay+1, state ? "ON" : "OFF", relayData);
  }
}

void readInputs() {
  for (int i = 0; i < 8; i++) {
    // INPUT_PULLUP: logique inversée (0 = activé, 1 = inactif)
    inputStates[i] = !digitalRead(digitalInputs[i]);
  }
}

void readSensors() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  
  if (isnan(temperature)) temperature = 0.0;
  if (isnan(humidity)) humidity = 0.0;
}

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

// ===== FONCTIONS MQTT =====
void publishStatus() {
  if (!mqttClient.connected()) return;
  
  DynamicJsonDocument doc(512);
  doc["ip"] = Ethernet.localIP().toString();
  doc["uptime"] = millis() / 1000;
  doc["ethernet"] = (Ethernet.linkStatus() == LinkON) ? "OK" : "ERROR";
  doc["i2c"] = "OK";
  
  String payload;
  serializeJson(doc, payload);
  mqttClient.publish(TOPIC_STATUS, payload.c_str());
}

void publishRelayStates() {
  if (!mqttClient.connected()) return;
  
  DynamicJsonDocument doc(256);
  JsonArray relays = doc.createNestedArray("relays");
  for (int i = 0; i < 8; i++) {
    relays.add(relayStates[i] ? 1 : 0);
  }
  
  String payload;
  serializeJson(doc, payload);
  mqttClient.publish(TOPIC_RELAY_STATE, payload.c_str());
}

void publishInputStates() {
  if (!mqttClient.connected()) return;
  
  DynamicJsonDocument doc(256);
  JsonArray inputs = doc.createNestedArray("inputs");
  for (int i = 0; i < 8; i++) {
    inputs.add(inputStates[i] ? 1 : 0);
  }
  
  String payload;
  serializeJson(doc, payload);
  mqttClient.publish(TOPIC_INPUT_STATE, payload.c_str());
}

void publishSensorData() {
  if (!mqttClient.connected()) return;
  
  DynamicJsonDocument doc(256);
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["timestamp"] = millis() / 1000;
  
  String payload;
  serializeJson(doc, payload);
  mqttClient.publish(TOPIC_SENSOR, payload.c_str());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.printf("🔥 MQTT REÇU: Topic='%s' Message='%s'\n", topic, message.c_str());
  
  if (String(topic) == TOPIC_RELAY_CMD) {
    Serial.println("📡 Traitement commande relais...");
    
    // Vérifier si c'est du JSON ou format simple
    if (message.startsWith("{")) {
      // Format JSON: {"relay": 1, "state": "on"}
      DynamicJsonDocument doc(200);
      DeserializationError error = deserializeJson(doc, message);
      
      if (error) {
        Serial.printf("❌ Erreur parsing JSON: %s\n", error.c_str());
        return;
      }
      
      int relayNum = doc["relay"];
      String stateStr = doc["state"];
      
      Serial.printf("   JSON - Relais: %d, État: '%s'\n", relayNum, stateStr.c_str());
      
      if (relayNum >= 1 && relayNum <= 8) {
        bool state = (stateStr == "on");
        Serial.printf("🎯 Exécution: setRelay(%d, %s)\n", relayNum - 1, state ? "true" : "false");
        setRelay(relayNum - 1, state);
        Serial.printf("✅ MQTT: Relais %d %s\n", relayNum, state ? "ON" : "OFF");
        publishRelayStates();
      } else {
        Serial.printf("❌ Numéro relais invalide: %d\n", relayNum);
      }
    } else {
      // Format simple: "1:ON" ou "1:OFF"
      int colonIndex = message.indexOf(':');
      if (colonIndex > 0) {
        String relayStr = message.substring(0, colonIndex);
        String stateStr = message.substring(colonIndex + 1);
        
        Serial.printf("   Simple - Relais: '%s', État: '%s'\n", relayStr.c_str(), stateStr.c_str());
        
        if (relayStr == "ALL") {
          bool state = (stateStr == "ON");
          Serial.printf("   Commande TOUS les relais: %s\n", state ? "ON" : "OFF");
          for (int i = 0; i < 8; i++) {
            setRelay(i, state);
          }
          Serial.printf("MQTT: Tous les relais %s\n", state ? "ON" : "OFF");
        } else {
          int relayNum = relayStr.toInt();
          Serial.printf("   Numéro relais parsé: %d\n", relayNum);
          if (relayNum >= 1 && relayNum <= 8) {
            bool state = (stateStr == "ON");
            Serial.printf("   Commande relais %d: %s\n", relayNum, state ? "ON" : "OFF");
            setRelay(relayNum - 1, state);
            Serial.printf("MQTT: Relais %d %s\n", relayNum, state ? "ON" : "OFF");
          } else {
            Serial.printf("❌ Numéro relais invalide: %d\n", relayNum);
          }
        }
        publishRelayStates();
      } else {
        Serial.printf("❌ Format message invalide: %s\n", message.c_str());
      }
    }
  } else {
    Serial.printf("❌ Topic non reconnu: %s\n", topic);
  }
}

void connectMQTT() {
  if (millis() - lastMqttReconnect < 5000) return; // Éviter trop de tentatives
  lastMqttReconnect = millis();
  
  Serial.print("Connexion MQTT...");
  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
    Serial.println(" ✓");
    mqttClient.subscribe(TOPIC_RELAY_CMD);
    Serial.println("✓ Abonné aux commandes relais");
    
    // Publier état initial
    publishStatus();
    publishRelayStates();
    publishInputStates();
  } else {
    Serial.printf(" ✗ (code %d)\n", mqttClient.state());
  }
}

void setup() {
  // Initialisation série avec délai pour démarrage
  Serial.begin(9600);
  delay(2000);
  
  Serial.println("\n\n=== DÉMARRAGE ESP32-S3-ETH-8DI-8RO ===");
  Serial.println("Version: MQTT + Interface Web");
  Serial.println("Date: " + String(__DATE__) + " " + String(__TIME__));
  
  // Test de base
  for (int i = 0; i < 5; i++) {
    Serial.printf("Test %d/5\n", i+1);
    delay(500);
  }
  
  // Initialisation I2C avec pins Waveshare officiels
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
  
  // Vérification Ethernet
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println(" ✗");
    Serial.println("❌ W5500 non détecté avec ces pins!");
  } else if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println(" ⚠️");
    Serial.println("⚠️  W5500 détecté mais pas de câble");
  } else {
    Serial.println(" ✓");
    Serial.print("✓ IP Ethernet: ");
    Serial.println(Ethernet.localIP());
  }
  
  // Initialisation MQTT
  mqttClient.setServer(mqttServer, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  Serial.printf("✓ MQTT configuré: %s:%d\n", mqttServer.toString().c_str(), MQTT_PORT);
  
  Serial.println("\n=== Système prêt ===");
  Serial.println("Fonctionnalités disponibles:");
  Serial.println("- Interface web: http://" + Ethernet.localIP().toString());
  Serial.println("- MQTT: " + mqttServer.toString() + ":" + String(MQTT_PORT));
  Serial.println("- Commandes série: tapez 'help'");
}

void loop() {
  Ethernet.maintain();
  
  // Connexion MQTT si nécessaire
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();
  
  // Lecture des capteurs et entrées toutes les 2 secondes
  static uint32_t lastSensorRead = 0;
  if (millis() - lastSensorRead > 2000) {
    lastSensorRead = millis();
    readSensors();
    readInputs();
    
    // Vérifier changements d'entrées pour MQTT
    bool inputChanged = false;
    for (int i = 0; i < 8; i++) {
      if (inputStates[i] != lastInputStates[i]) {
        inputChanged = true;
        lastInputStates[i] = inputStates[i];
      }
    }
    if (inputChanged) {
      publishInputStates();
    }
  }
  
  // Publication MQTT périodique (toutes les 30 secondes)
  if (mqttClient.connected() && (millis() - lastMqttPublish > 30000)) {
    lastMqttPublish = millis();
    publishSensorData();
    publishStatus();
  }
  
  // Commandes série pour debug et contrôle immédiat
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    if (command.startsWith("relay ")) {
      int relayNum = command.substring(6, 7).toInt();
      String state = command.substring(8);
      
      if (relayNum >= 1 && relayNum <= 8) {
        if (state == "on") {
          setRelay(relayNum - 1, true);
          Serial.printf("Relais %d: ON\n", relayNum);
          publishRelayStates();
        } else if (state == "off") {
          setRelay(relayNum - 1, false);
          Serial.printf("Relais %d: OFF\n", relayNum);
          publishRelayStates();
        }
      }
    }
    else if (command == "status") {
      Serial.println("\n=== STATUS SYSTÈME ===");
      Serial.printf("Uptime: %d secondes\n", millis()/1000);
      
      // Ethernet
      if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        Serial.println("Ethernet W5500: ✗ ERREUR");
      } else if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println("Ethernet W5500: ⚠️ PAS DE CÂBLE");
      } else {
        Serial.printf("Ethernet W5500: ✓ OK (%s)\n", Ethernet.localIP().toString().c_str());
      }
      
      // MQTT
      Serial.printf("MQTT: %s\n", mqttClient.connected() ? "✓ Connecté" : "✗ Déconnecté");
      
      // I2C/TCA9554
      Wire.beginTransmission(TCA9554_ADDR);
      uint8_t i2cResult = Wire.endTransmission();
      Serial.printf("TCA9554 I2C: %s\n", i2cResult == 0 ? "✓ OK" : "✗ ERREUR");
      
      // Capteurs
      Serial.printf("Température: %.1f°C\n", temperature);
      Serial.printf("Humidité: %.1f%%\n", humidity);
      
      // Relais
      Serial.print("Relais: ");
      for (int i = 0; i < 8; i++) {
        Serial.printf("%d:%s ", i+1, relayStates[i] ? "ON" : "OFF");
      }
      Serial.println();
      
      // Entrées
      Serial.print("Entrées: ");
      for (int i = 0; i < 8; i++) {
        Serial.printf("%d:%s ", i+1, inputStates[i] ? "HIGH" : "LOW");
      }
      Serial.println();
      Serial.println("=======================");
    }
    else if (command == "scan") {
      scanI2C();
    }
    else if (command == "testio") {
      Serial.println("\n=== TEST ENTRÉES/SORTIES ===");
      
      // Test des entrées
      readInputs();
      Serial.println("📥 État des entrées digitales:");
      for (int i = 0; i < 8; i++) {
        Serial.printf("  Entrée %d (pin %d): %s\n", i+1, digitalInputs[i], inputStates[i] ? "HIGH" : "LOW");
      }
      
      // Test séquentiel des relais si TCA9554 OK
      Wire.beginTransmission(TCA9554_ADDR);
      uint8_t i2cResult = Wire.endTransmission();
      
      if (i2cResult == 0) {
        Serial.println("🔌 Test séquentiel des relais:");
        for (int i = 0; i < 8; i++) {
          Serial.printf("  Test relais %d...", i+1);
          setRelay(i, true);
          delay(500);
          setRelay(i, false);
          delay(200);
          Serial.println(" OK");
        }
        Serial.println("✓ Test relais terminé");
      } else {
        Serial.println("❌ TCA9554 inaccessible - relais non testés");
      }
      Serial.println("===============================");
    }
    else if (command == "mqtttest") {
      Serial.println("\n=== DIAGNOSTIC MQTT ===");
      Serial.printf("Broker: %s:%d\n", mqttServer.toString().c_str(), MQTT_PORT);
      Serial.printf("Client ID: %s\n", MQTT_CLIENT_ID);
      Serial.printf("User: %s\n", MQTT_USER);
      Serial.printf("Connecté: %s\n", mqttClient.connected() ? "✓ OUI" : "✗ NON");
      
      if (mqttClient.connected()) {
        Serial.println("📡 Test publication...");
        bool result = mqttClient.publish("esp32s3/test", "Hello from ESP32");
        Serial.printf("Publication test: %s\n", result ? "✓ OK" : "✗ ÉCHEC");
        
        Serial.println("📡 État abonnements:");
        Serial.println("  - esp32s3/relay/cmd (souscrit au démarrage)");
      } else {
        Serial.printf("Erreur connexion: %d\n", mqttClient.state());
        Serial.println("Codes erreur MQTT:");
        Serial.println("  -4: Connection timeout");
        Serial.println("  -3: Connection lost");
        Serial.println("  -2: Connect failed");
        Serial.println("  -1: Disconnected");
        Serial.println("   0: Connected");
        Serial.println("   1: Bad protocol");
        Serial.println("   2: Bad client ID");
        Serial.println("   3: Unavailable");
        Serial.println("   4: Bad credentials");
        Serial.println("   5: Unauthorized");
      }
      Serial.println("========================");
    }
    else if (command == "testrelays") {
      Serial.println("\n=== TEST LOGIQUE RELAIS ===");
      Serial.println("Test 1: Écriture 0x00...");
      tca9554_write(0x00);
      delay(2000);
      Serial.println("Test 2: Écriture 0xFF...");
      tca9554_write(0xFF);
      delay(2000);
      Serial.println("Test 3: Écriture 0x01 (relais 1)...");
      tca9554_write(0x01);
      delay(2000);
      Serial.println("Test 4: Écriture 0xFE (tous sauf relais 1)...");
      tca9554_write(0xFE);
      delay(2000);
      Serial.println("Test terminé - observez quand les relais s'activent");
      Serial.println("=============================");
    }
    else if (command == "forceoff") {
      Serial.println("\n=== FORCE TOUS RELAIS OFF ===");
      // Reset des états logiciels
      for (int i = 0; i < 8; i++) {
        relayStates[i] = false;
      }
      // Utiliser la BONNE logique maintenant identifiée
      Serial.println("Arrêt avec 0x00 (logique confirmée)...");
      tca9554_write(0x00);
      Serial.println("✓ Tous les relais arrêtés");
      Serial.println("==============================");
    }
    else if (command == "help") {
      Serial.println("\n=== COMMANDES DISPONIBLES ===");
      Serial.println("📋 Informations:");
      Serial.println("  help           - Cette aide");
      Serial.println("  status         - Status détaillé complet");
      Serial.println("  scan           - Scan I2C des devices");
      Serial.println("  testio         - Test entrées et relais");
      Serial.println("  mqtttest       - Diagnostic MQTT détaillé");
      Serial.println("🔌 Contrôle des relais:");
      Serial.println("  relay X on/off - Contrôler relais 1-8");
      Serial.println("  testrelays     - Test logique relais (observer physiquement)");
      Serial.println("  forceoff       - Force arrêt tous relais");
      Serial.println("🌐 Accès réseau:");
      Serial.println("  Interface web: http://" + Ethernet.localIP().toString());
      Serial.println("  MQTT: " + mqttServer.toString() + ":" + String(MQTT_PORT));
      Serial.println("==============================");
    }
  }
}