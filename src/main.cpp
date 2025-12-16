#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <DHT.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <SPIFFS.h>
#include "web_config.h"

// Configuration des pins - WAVESHARE OFFICIEL VALIDÉ
#define ETH_SCK_PIN  15   // SPI Clock
#define ETH_MISO_PIN 14   // SPI MISO
#define ETH_MOSI_PIN 13   // SPI MOSI
#define ETH_CS_PIN   16   // W5500 Chip Select
#define ETH_RST_PIN  39   // W5500 Reset
#define ETH_IRQ_PIN  12   // W5500 Interrupt

#define DHT_PIN      40   // DHT22 Data
#define DHT_TYPE     DHT22

#define I2C_SDA_PIN  42   // TCA9554 SDA (I2C) - WAVESHARE OFFICIEL
#define I2C_SCL_PIN  41   // TCA9554 SCL (I2C) - WAVESHARE OFFICIEL
#define TCA9554_ADDR 0x20 // Adresse TCA9554

// MAC Ethernet
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress staticIP(192, 168, 1, 50);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns1(8, 8, 8, 8);

// Configuration MQTT (Mosquitto @ 192.168.1.200:1883)
IPAddress mqttServer(192, 168, 1, 200);
uint16_t mqttPort = 1883;
char mqttUser[50] = "pascal";
char mqttPassword[50] = "123456";
const char* mqttClientID = "ESP32-S3-ETH";
const char* CONFIG_FILE = "/config.json";

// MQTT Topics
char topicRelayCmd[100] = "home/esp32/relay/cmd";
char topicRelayStatus[100] = "home/esp32/relay/status";
char topicInputStatus[100] = "home/esp32/input/status";
char topicSensorStatus[100] = "home/esp32/sensor/status";
char topicSystemStatus[100] = "home/esp32/system/status";

// Pins des entrées digitales
const int digitalInputs[8] = {4, 5, 6, 7, 8, 9, 10, 11};

// DHT22 Sensor
DHT dht(DHT_PIN, DHT_TYPE);

// État de l'application
float temperature = 0.0;
float humidity = 0.0;
bool relayStates[8] = {false};
bool inputStates[8] = {true};
bool serverStarted = false;

// Variables pour serveur HTTP simple
EthernetClient clients[4];  // Max 4 clients simultanés
uint16_t httpPort = 80;

// MQTT Client
EthernetClient ethClient;
PubSubClient mqttClient(ethClient);
bool mqttConnected = false;
unsigned long lastMqttPublish = 0;
const unsigned long mqttPublishInterval = 5000;  // Publier chaque 5s

// ===== FONCTIONS FORWARD =====
void setRelay(int relay, bool state);
void readInputs();
void readSensors();
String getHtmlPage();
void handleHttpConnections();
void setupWebServer();
void setupMqtt();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void mqttPublishStatus();
void mqttReconnect();

// ===== FONCTIONS IMPLÉMENTATION =====

void setRelay(int relay, bool state) {
  if (relay >= 0 && relay < 8) {
    relayStates[relay] = state;
    
    // Lire l'état actuel du TCA9554
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); // Output port register
    Wire.endTransmission();
    Wire.requestFrom(TCA9554_ADDR, 1);
    byte output = Wire.read();
    
    // Modifier le bit du relais (inversion: ON = LOW, OFF = HIGH)
    if (state) {
      output &= ~(1 << relay); // SET bit LOW pour activé
    } else {
      output |= (1 << relay);  // SET bit HIGH pour désactivé
    }
    
    // Écrire de retour au registre output
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); // Output port register
    Wire.write(output);
    Wire.endTransmission();
    
    Serial.printf("✓ Relais %d: %s (TCA9554 @ 0x%02X bit %d)\n", relay+1, state ? "ON" : "OFF", TCA9554_ADDR, relay);
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

String getHtmlPage() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32-8DI8RO Dashboard</title>";
  html += "<style>";
  html += "* {margin:0;padding:0;box-sizing:border-box;}";
  html += "body {font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif;background:#f5f5f5;color:#333;}";
  html += "header {background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;padding:20px;text-align:center;}";
  html += ".container {max-width:1200px;margin:0 auto;padding:20px;}";
  html += ".card {background:white;border-radius:8px;box-shadow:0 2px 8px rgba(0,0,0,0.1);padding:20px;margin:15px 0;}";
  html += ".grid2 {display:grid;grid-template-columns:1fr 1fr;gap:20px;}";
  html += ".grid4 {display:grid;grid-template-columns:repeat(4,1fr);gap:15px;}";
  html += ".stat {text-align:center;padding:15px;background:#f9f9f9;border-radius:5px;}";
  html += ".stat-value {font-size:24px;font-weight:bold;color:#667eea;}";
  html += ".stat-label {font-size:12px;color:#999;margin-top:5px;}";
  html += ".relay-item {padding:15px;border:2px solid #ddd;border-radius:5px;text-align:center;transition:all 0.3s;}";
  html += ".relay-item:hover {border-color:#667eea;background:#f0f0ff;}";
  html += ".relay-item.on {background:#c8e6c9;border-color:#4CAF50;}";
  html += ".relay-item.off {background:#ffcccc;border-color:#f44336;}";
  html += ".relay-num {font-size:14px;color:#999;margin-bottom:10px;}";
  html += ".relay-status {font-size:18px;font-weight:bold;margin:10px 0;}";
  html += ".relay-btn {padding:8px 16px;border:none;border-radius:4px;cursor:pointer;font-weight:bold;font-size:12px;width:100%;transition:all 0.3s;}";
  html += ".relay-btn.on {background:#4CAF50;color:white;}";
  html += ".relay-btn.off {background:#f44336;color:white;}";
  html += ".relay-btn:hover {opacity:0.8;}";
  html += ".relay-btn:active {transform:scale(0.98);}";
  html += ".input-item {padding:15px;border:2px solid #ddd;border-radius:5px;text-align:center;}";
  html += ".input-item.high {background:#c8e6c9;border-color:#4CAF50;}";
  html += ".input-item.low {background:#ffcccc;border-color:#f44336;}";
  html += ".input-num {font-size:14px;color:#999;margin-bottom:10px;}";
  html += ".input-status {font-size:16px;font-weight:bold;margin:10px 0;}";
  html += "h2 {color:#333;margin:20px 0 15px 0;font-size:18px;border-bottom:2px solid #667eea;padding-bottom:10px;}";
  html += ".status-bar {background:#fff;padding:10px;border-radius:5px;font-size:12px;color:#666;margin-top:20px;}";
  html += "footer {text-align:center;color:#999;margin-top:40px;padding:20px;font-size:12px;}";
  html += "</style></head><body>";
  html += "<header><h1>⚡ ESP32-S3-ETH-8DI8RO Dashboard</h1></header>";
  html += "<div class='container'>";
  
  // Section Capteurs
  html += "<div class='card'>";
  html += "<h2>📊 Capteurs</h2>";
  html += "<div class='grid2'>";
  html += "<div class='stat'><div class='stat-value'>";
  html += String(temperature, 1);
  html += "°C</div><div class='stat-label'>🌡️ Température</div></div>";
  html += "<div class='stat'><div class='stat-value'>";
  html += String(humidity, 1);
  html += "%</div><div class='stat-label'>💧 Humidité</div></div>";
  html += "</div></div>";
  
  // Section Relais
  html += "<div class='card'>";
  html += "<h2>⚡ Relais (8)</h2>";
  html += "<div class='grid4'>";
  for (int i = 0; i < 8; i++) {
    bool isOn = relayStates[i];
    html += "<div class='relay-item ";
    html += (isOn ? "on" : "off");
    html += "'><div class='relay-num'>Relais ";
    html += String(i+1);
    html += "</div><div class='relay-status'>";
    html += (isOn ? "ON" : "OFF");
    html += "</div>";
    html += "<form method='POST' action='/api/relay/";
    html += String(i);
    html += (isOn ? "/off' style='display:inline;width:100%'>" : "/on' style='display:inline;width:100%'>");
    html += "<button type='submit' class='relay-btn ";
    html += (isOn ? "on" : "off");
    html += "'>";
    html += (isOn ? "Éteindre" : "Allumer");
    html += "</button></form></div>";
  }
  html += "</div></div>";
  
  // Section Entrées
  html += "<div class='card'>";
  html += "<h2>📥 Entrées Digitales (8)</h2>";
  html += "<div class='grid4'>";
  for (int i = 0; i < 8; i++) {
    bool isHigh = inputStates[i];
    html += "<div class='input-item ";
    html += (isHigh ? "high" : "low");
    html += "'><div class='input-num'>Entrée ";
    html += String(i+1);
    html += "</div><div class='input-status'>";
    html += (isHigh ? "HIGH" : "LOW");
    html += "</div></div>";
  }
  html += "</div></div>";
  
  html += "<div class='card status-bar'>";
  html += "Système opérationnel | Ethernet W5500 | Rafraîchissement: 3s";
  html += "</div>";
  
  html += "</div>";
  html += "<script>setTimeout(function(){location.reload();},3000);</script>";
  html += "<footer>API JSON: /api/status | Contrôle: /api/relay/X/on ou /api/relay/X/off</footer>";
  html += "</body></html>";
  
  return html;
}

// ===== FONCTIONS MQTT =====

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Traiter les commandes reçues via MQTT
  String topicStr(topic);
  String payloadStr("");
  for (unsigned int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }
  
  Serial.printf("\n📨 MQTT Reçu:\n");
  Serial.printf("   Topic: %s\n", topic);
  Serial.printf("   Payload: %s\n", payloadStr.c_str());
  Serial.printf("   Expected: %s\n", topicRelayCmd);
  Serial.printf("   Match: %s\n", (topicStr == topicRelayCmd) ? "OUI" : "NON");
  
  // Parser commandes de relais: home/esp32/relay/cmd
  if (topicStr == topicRelayCmd) {
    Serial.println("✓ Topic reconnu - parsing...");
    
    // Format: "0:on" ou "1:off" ou "ALL:on"
    int colonPos = payloadStr.indexOf(':');
    Serial.printf("   Colon position: %d\n", colonPos);
    
    if (colonPos > 0) {
      String relayStr = payloadStr.substring(0, colonPos);
      String stateStr = payloadStr.substring(colonPos + 1);
      stateStr.toLowerCase();
      
      Serial.printf("   Relay: %s, State: %s\n", relayStr.c_str(), stateStr.c_str());
      
      bool isOn = (stateStr == "on" || stateStr == "1");
      
      if (relayStr == "ALL") {
        // Tous les relais
        for (int i = 0; i < 8; i++) {
          setRelay(i, isOn);
        }
        Serial.println("✓ Tous les relais: " + stateStr);
      } else {
        // Relais spécifique
        int relayNum = relayStr.toInt();
        Serial.printf("   Relais numero: %d\n", relayNum);
        
        if (relayNum >= 0 && relayNum < 8) {
          setRelay(relayNum, isOn);
          Serial.printf("✓ Relais %d: %s\n", relayNum, stateStr.c_str());
        } else {
          Serial.printf("✗ Numero relais invalide: %d\n", relayNum);
        }
      }
    } else {
      Serial.println("✗ Format invalide (attendu: 0:on ou ALL:off)");
    }
  } else {
    Serial.println("✗ Topic non reconnu");
  }
}

void mqttPublishStatus() {
  if (!mqttClient.connected()) return;
  
  // État des relais
  StaticJsonDocument<256> relayDoc;
  for (int i = 0; i < 8; i++) {
    relayDoc[i] = relayStates[i] ? 1 : 0;
  }
  String relayStatus;
  serializeJson(relayDoc, relayStatus);
  mqttClient.publish(topicRelayStatus, relayStatus.c_str());
  
  // État des entrées
  StaticJsonDocument<256> inputDoc;
  for (int i = 0; i < 8; i++) {
    inputDoc[i] = inputStates[i] ? 1 : 0;
  }
  String inputStatus;
  serializeJson(inputDoc, inputStatus);
  mqttClient.publish(topicInputStatus, inputStatus.c_str());
  
  // État des capteurs
  StaticJsonDocument<128> sensorDoc;
  sensorDoc["temperature"] = temperature;
  sensorDoc["humidity"] = humidity;
  String sensorStatus;
  serializeJson(sensorDoc, sensorStatus);
  mqttClient.publish(topicSensorStatus, sensorStatus.c_str());
  
  // État du système
  StaticJsonDocument<128> systemDoc;
  systemDoc["ip"] = Ethernet.localIP().toString();
  systemDoc["mqtt"] = "connected";
  systemDoc["uptime"] = millis() / 1000;
  String systemStatus;
  serializeJson(systemDoc, systemStatus);
  mqttClient.publish(topicSystemStatus, systemStatus.c_str());
}

void mqttReconnect() {
  // Reconnecter au broker MQTT
  int attempts = 0;
  const int maxAttempts = 3;
  
  while (!mqttClient.connected() && attempts < maxAttempts) {
    Serial.print("Tentative MQTT... ");
    
    if (mqttClient.connect(mqttClientID, mqttUser, mqttPassword)) {
      Serial.println("✓ Connecté");
      mqttConnected = true;
      
      // Souscrire aux topics de commande
      mqttClient.subscribe(topicRelayCmd);
      Serial.println("✓ Souscrit à " + String(topicRelayCmd));
      
      // Publier l'état initial
      mqttPublishStatus();
    } else {
      Serial.printf("✗ Erreur: code %d\n", mqttClient.state());
      delay(3000);
      attempts++;
    }
  }
  
  if (!mqttClient.connected()) {
    mqttConnected = false;
  }
}

void setupMqtt() {
  Serial.println("\n=== Configuration MQTT ===");
  
  // Configurer le serveur MQTT
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);
  
  Serial.printf("Broker: %d.%d.%d.%d:%d\n", 
    mqttServer[0], mqttServer[1], mqttServer[2], mqttServer[3], mqttPort);
  
  // Tentative de connexion
  mqttReconnect();
}

// Gestionnaire simple de connexions HTTP (stub pour maintenant)
// Serveur HTTP simple avec Ethernet
void handleHttpConnections() {
  // Implémentation minimale - sera améliorée après validation de stabilité
  // Note: EthernetServer.available() sur ESP32 nécessite une configuration lwip spéciale
}

void setupWebServer() {
  // Serveur HTTP basique - à implémenter après validation v1.5
  Serial.println("✓ HTTP server ready (à implémenter)");
}


void setup() {
  Serial.begin(9600);
  delay(3000);  // Extra long delay for monitor to connect
  
  Serial.write(0x00); Serial.write(0xFF);  // Flush
  delay(500);
  Serial.println("\n\n╔════════════════════════════════════════╗");
  Serial.println("║ === DÉMARRAGE ESP32-S3-ETH-8DI-8RO ═══ ║");
  Serial.println("╚════════════════════════════════════════╝");
  delay(500);
  
  // Initialisation I2C pour TCA9554
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN); // SDA=42, SCL=41 (Waveshare official)
  Wire.setClock(100000);
  Serial.println("✓ I2C initialisé (SDA=42, SCL=41)");
  
  // Configuration TCA9554 @ 0x20
  delay(100); // Laisser TCA9554 initialiser
  
  // Configuration des pins: 0x03 = register config (polarity inversion)
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(0x03); // Polarity inversion register
  Wire.write(0x00); // Pas d'inversion
  Wire.endTransmission();
  
  // 0x07 = Output register - initialiser tous les relais à OFF (HIGH logique inversée)
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(0x07); // Output register
  Wire.write(0xFF); // Tous les outputs à 1 (relais OFF)
  Wire.endTransmission();
  
  // 0x06 = IO configuration register (0=output, 1=input)
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(0x06); // Config register
  Wire.write(0x00); // Tous les pins en OUTPUT mode (relais)
  Wire.endTransmission();
  
  Serial.println("✓ TCA9554 configuré (8 relais OUTPUT)");
  
  Wire.beginTransmission(0x20);
  Wire.write(0x01); // Output register
  Wire.write(0x00); // Tous les relais OFF au démarrage
  Wire.endTransmission();
  Serial.println("✓ TCA9554 (I2C @ 0x20) configuré");
  
  // Initialisation DHT22
  dht.begin();
  Serial.println("✓ DHT22 initialisé");
  
  // Configuration des entrées digitales
  for (int i = 0; i < 8; i++) {
    pinMode(digitalInputs[i], INPUT_PULLUP);
  }
  Serial.println("✓ Entrées digitales configurées");

  // Initialisation SPI pour W5500
  Serial.println("Configuration SPI...");
  SPI.begin(ETH_SCK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, ETH_CS_PIN);
  Serial.printf("✓ SPI: SCK=%d, MISO=%d, MOSI=%d, CS=%d\n", 
                ETH_SCK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, ETH_CS_PIN);

  // Configuration des pins
  pinMode(ETH_CS_PIN, OUTPUT);
  pinMode(ETH_RST_PIN, OUTPUT);
  pinMode(ETH_IRQ_PIN, INPUT);
  
  digitalWrite(ETH_CS_PIN, HIGH);
  
  // Reset du W5500
  Serial.printf("Reset W5500 (pin %d)...\n", ETH_RST_PIN);
  digitalWrite(ETH_RST_PIN, LOW);
  delay(100);
  digitalWrite(ETH_RST_PIN, HIGH);
  delay(1000);
  
  Serial.println("✓ Reset W5500 terminé");
  
  // Initialisation Ethernet W5500
  Ethernet.init(ETH_CS_PIN);
  Serial.print("Initialisation Ethernet...");
  Ethernet.begin(mac, staticIP, dns1, gateway, subnet);
  delay(2000);
  
  // Vérification statut Ethernet
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println(" ✗");
    Serial.println("❌ W5500 non détecté!");
  } else if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println(" ⚠️");
    Serial.println("⚠️ W5500 détecté mais pas de câble");
  } else {
    Serial.println(" ✓");
    Serial.print("✓ IP Ethernet: ");
    Serial.println(Ethernet.localIP());
  }
  
  serverStarted = true;
  Serial.println("✓ Serveur HTTP sur port 80");
  Serial.print("  Accès: http://");
  Serial.println(Ethernet.localIP());
  
  // Initialisation SPIFFS et chargement config MQTT
  initSPIFFS();
  loadMQTTConfig();
  
  // Configuration MQTT
  setupMqtt();
  
  Serial.println("\n=== Système prêt ===\n");
  Serial.println("Accès interface web: http://");
  Serial.print(Ethernet.localIP());
  Serial.println("/config");
}

void loop() {
  // Maintenir la connexion Ethernet
  Ethernet.maintain();
  
  // Gestion HTTP
  handleWebServer(clients[0]);
  
  // Gestion MQTT
  if (!mqttClient.connected()) {
    Serial.println("✗ MQTT Déconnecté - reconnexion...");
    mqttReconnect();
  } else {
    mqttClient.loop();  // Process incoming messages
    Serial.print(".");  // Signal que le loop tourne
    
    // Publier l'état régulièrement
    if (millis() - lastMqttPublish >= mqttPublishInterval) {
      lastMqttPublish = millis();
      mqttPublishStatus();
    }
  }
  
  // Traitement des commandes sériales
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    // Parse commands: "relay X on/off" or "test"
    if (cmd.startsWith("relay ")) {
      int spaceIdx1 = cmd.indexOf(' ', 6);
      if (spaceIdx1 > 0) {
        int relayNum = cmd.substring(6, spaceIdx1).toInt();
        String state = cmd.substring(spaceIdx1 + 1);
        bool isOn = (state == "on");
        
        if (relayNum >= 0 && relayNum < 8) {
          Serial.printf("Test relay %d: %s\n", relayNum, isOn ? "ON" : "OFF");
          setRelay(relayNum, isOn);
          delay(100);
          Serial.printf("✓ Relay %d set to %s\n", relayNum, isOn ? "ON" : "OFF");
        } else {
          Serial.println("Erreur: relay num 0-7");
        }
      }
    } else if (cmd == "test") {
      Serial.println("\n=== TEST RELAIS (0-7) ===");
      for (int i = 0; i < 8; i++) {
        Serial.printf("Allumage relais %d...\n", i);
        setRelay(i, true);
        delay(500);
        setRelay(i, false);
      }
      Serial.println("✓ Test complet\n");
    } else if (cmd == "help") {
      Serial.println("\nCommandes disponibles:");
      Serial.println("  relay X on/off  - Allume/éteint relais X (0-7)");
      Serial.println("  test            - Test tous les relais");
      Serial.println("  help            - Affiche cette aide\n");
    }
  }
  
  // Lecture des capteurs et entrées toutes les 2 secondes
  static uint32_t lastSensorRead = 0;
  if (millis() - lastSensorRead > 2000) {
    lastSensorRead = millis();
    readSensors();
    readInputs();
    
    Serial.printf("Temp=%.1f°C Hum=%.1f%% | Relais: ", temperature, humidity);
    for (int i = 0; i < 8; i++) Serial.printf("%d ", relayStates[i] ? 1 : 0);
    Serial.print("| Entrées: ");
    for (int i = 0; i < 8; i++) Serial.printf("%d ", inputStates[i] ? 1 : 0);
    Serial.println();
  }
}
