#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <DHT.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <SPIFFS.h>
#include <Update.h>
#include "web_config.h"

#ifndef ENABLE_OTA_HTTP
#define ENABLE_OTA_HTTP 0
#endif

// ESP32 Arduino core's Server interface requires begin(uint16_t).
// The Arduino Ethernet library's EthernetServer implements begin() with no args.
// Adapter to bridge the signature mismatch.
class EthernetServerESP32 : public EthernetServer {
public:
  explicit EthernetServerESP32(uint16_t port) : EthernetServer(port) {}
  void begin(uint16_t port = 0) override {
    (void)port;
    EthernetServer::begin();
  }
};

// Configuration des pins - WAVESHARE OFFICIEL VALIDÉ
#define ETH_SCK_PIN  15   // SPI Clock
#define ETH_MISO_PIN 14   // SPI MISO
#define ETH_MOSI_PIN 13   // SPI MOSI
#define ETH_CS_PIN   16   // W5500 Chip Select
#define ETH_RST_PIN  39   // W5500 Reset
#define ETH_IRQ_PIN  12   // W5500 Interrupt

#define DHT_PIN      21   // DHT22 Data (connecteur interne: GPIO21)
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
// Ne pas stocker de credentials en dur dans le firmware (configurable via SPIFFS / interface Web).
char mqttUser[50] = "";
char mqttPassword[50] = "";
// Clé OTA (optionnelle). Si vide: OTA refusée.
char otaKey[64] = "";
const char* mqttClientID = "ESP32-S3-ETH";
const char* CONFIG_FILE = "/config.json";

// MQTT Topics
char topicRelayCmd[100] = "waveshare/relay/cmd";
char topicRelayStatus[100] = "waveshare/relay/status";
char topicInputStatus[100] = "waveshare/input/status";
char topicSensorStatus[100] = "waveshare/sensor/status";
char topicSystemStatus[100] = "waveshare/system/status";

// Pins des entrées digitales
const int digitalInputs[8] = {4, 5, 6, 7, 8, 9, 10, 11};

// DHT22 Sensor
DHT dht(DHT_PIN, DHT_TYPE);

// État de l'application
float temperature = 0.0;
float humidity = 0.0;
bool relayStates[8] = {false};
bool inputStates[8] = {false};
bool serverStarted = false;

// Variables pour serveur HTTP simple
uint16_t httpPort = 80;
EthernetServerESP32 webServer(80);

// MQTT Client
EthernetClient ethClient;
PubSubClient mqttClient(ethClient);
bool mqttConnected = false;
unsigned long lastMqttPublish = 0;
const unsigned long mqttPublishInterval = 5000;  // Publier chaque 5s
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long mqttReconnectIntervalMs = 3000;

// ===== FONCTIONS FORWARD =====
void setRelay(int relay, bool state);
void readInputs();
void readSensors();
String getHtmlPage();
void handleHttpConnections();
void handleHttpLoop();
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
    
    // Modifier le bit du relais (logique active HIGH)
    // state=true => bit HIGH (relais ON)
    // state=false => bit LOW  (relais OFF)
    if (state) {
      output |= (1 << relay);
    } else {
      output &= ~(1 << relay);
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
    // Entrées en INPUT_PULLUP: actif = niveau bas (0)
    inputStates[i] = (digitalRead(digitalInputs[i]) == LOW);
  }
}

void readSensors() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  if (isnan(temperature)) temperature = 0.0;
  if (isnan(humidity)) humidity = 0.0;
}

String getHtmlPage() {
  String ipStr = Ethernet.localIP().toString();
  String cfgStaticIpStr = staticIP.toString();
  String cfgGatewayStr = gateway.toString();
  String cfgSubnetStr = subnet.toString();
  String cfgDnsStr = dns1.toString();

  String mqttStr = mqttServer.toString();
  String mqttState = mqttConnected ? "CONNECTE" : "DECONNECTE";

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32-8DI8RO Dashboard</title>";
  html += "<style>";
  html += "* {margin:0;padding:0;box-sizing:border-box;}";
  html += "body {font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif;background:#0b0f0d;color:#e6f2ea;}";
  html += "header {background:linear-gradient(135deg,#0f3d2e 0%,#072a1f 100%);color:#e6f2ea;padding:20px;text-align:center;}";
  html += ".container {max-width:1200px;margin:0 auto;padding:20px;}";
  html += ".card {background:#0f1a14;border-radius:6px;box-shadow:none;padding:20px;margin:15px 0;border:1px solid #1c5a41;}";
  html += ".grid2 {display:grid;grid-template-columns:1fr 1fr;gap:20px;}";
  html += ".grid4 {display:grid;grid-template-columns:repeat(4,1fr);gap:15px;}";
  html += ".stat {text-align:center;padding:15px;background:#0b0f0d;border-radius:5px;border:1px solid #1c5a41;}";
  html += ".stat-value {font-size:24px;font-weight:bold;color:#7ef7c8;}";
  html += ".stat-label {font-size:12px;color:#b7d7c8;margin-top:5px;}";
  html += ".relay-item {padding:15px;border:2px solid #1c5a41;border-radius:5px;text-align:center;transition:all 0.2s;}";
  html += ".relay-item:hover {border-color:#2ea87f;background:#0b0f0d;}";
  html += ".relay-item.on {background:#0c1f18;border-color:#2ea87f;}";
  html += ".relay-item.off {background:#0e1311;border-color:#2b3a33;}";
  html += ".relay-num {font-size:14px;color:#b7d7c8;margin-bottom:10px;}";
  html += ".relay-status {font-size:18px;font-weight:bold;margin:10px 0;}";
  html += ".relay-btn {padding:8px 16px;border:none;border-radius:4px;cursor:pointer;font-weight:bold;font-size:12px;width:100%;transition:all 0.3s;}";
  html += ".relay-btn.on {background:#2ea87f;color:#07130e;}";
  html += ".relay-btn.off {background:#4b5563;color:#e5e7eb;}";
  html += ".relay-btn:hover {opacity:0.8;}";
  html += ".relay-btn:active {transform:scale(0.98);}";
  html += ".input-item {padding:15px;border:2px solid #1c5a41;border-radius:5px;text-align:center;}";
  // Entrées: logique active-bas (ACTIVE=LOW=jaune, INACTIVE=HIGH=bleu)
  html += ".input-item.high {background:#0a1324;border-color:#2563eb;}";
  html += ".input-item.low {background:#1a1406;border-color:#d97706;}";
  html += ".input-num {font-size:14px;color:#b7d7c8;margin-bottom:10px;}";
  html += ".input-status {font-size:16px;font-weight:bold;margin:10px 0;}";
  html += "h2 {color:#e6f2ea;margin:20px 0 15px 0;font-size:18px;border-bottom:2px solid #2ea87f;padding-bottom:10px;}";
  html += ".status-bar {background:#0b0f0d;padding:10px;border-radius:5px;font-size:12px;color:#cfe9dc;margin-top:20px;border:1px solid #1c5a41;}";
  html += ".cfg-row {display:grid;grid-template-columns:1fr 1fr;gap:12px;}";
  html += ".cfg-field label {display:block;font-size:12px;color:#b7d7c8;margin-bottom:6px;}";
  html += ".cfg-field input {width:100%;padding:10px;border-radius:6px;border:1px solid #1c5a41;background:#0b0f0d;color:#e6f2ea;}";
  html += ".cfg-actions {margin-top:12px;display:flex;gap:10px;align-items:center;}";
  html += ".cfg-msg {font-size:12px;color:#cfe9dc;}";
  html += "footer {text-align:center;color:#b7d7c8;margin-top:40px;padding:20px;font-size:12px;}";
  html += "</style></head><body>";
  html += "<header><h1>ESP32-S3-ETH-8DI8RO Dashboard</h1></header>";
  html += "<div class='container'>";
  
  // Section Capteurs
  html += "<div class='card'>";
  html += "<h2>Capteurs</h2>";
  html += "<div class='grid2'>";
  html += "<div class='stat'><div class='stat-value' id='temp_val'>";
  html += String(temperature, 1);
  html += "°C</div><div class='stat-label'>Temperature</div></div>";
  html += "<div class='stat'><div class='stat-value' id='hum_val'>";
  html += String(humidity, 1);
  html += "%</div><div class='stat-label'>Humidite</div></div>";
  html += "</div></div>";

  // Section Système
  html += "<div class='card'>";
  html += "<h2>Systeme</h2>";
  html += "<div class='grid2'>";
  html += "<div class='stat'><div class='stat-value' id='sys_ip'>";
  html += ipStr;
  html += "</div><div class='stat-label'>IP Ethernet</div></div>";
  html += "<div class='stat'><div class='stat-value' id='sys_mqtt'>";
  html += mqttState;
  html += "</div><div class='stat-label'>MQTT (";
  html += mqttStr;
  html += ":";
  html += String(mqttPort);
  html += ")</div></div>";
  html += "</div></div>";
  
  // Section Relais
  html += "<div class='card'>";
  html += "<h2>Relais (8)</h2>";
  html += "<div class='grid4'>";
  for (int i = 0; i < 8; i++) {
    bool isOn = relayStates[i];
    html += "<div id='relay_";
    html += String(i + 1);
    html += "' class='relay-item ";
    html += (isOn ? "on" : "off");
    html += "'><div class='relay-num'>Relais ";
    html += String(i+1);
    html += "</div><div class='relay-status' id='relay_status_";
    html += String(i + 1);
    html += "'>";
    html += (isOn ? "ON" : "OFF");
    html += "</div>";
    html += "<button id='relay_btn_";
    html += String(i + 1);
    html += "' type='button' class='relay-btn ";
    html += (isOn ? "on" : "off");
    html += "' onclick='toggleRelay(";
    html += String(i + 1);
    html += ")'>";
    html += (isOn ? "Toggle (actuellement ON)" : "Toggle (actuellement OFF)");
    html += "</button></div>";
  }
  html += "</div></div>";

  html += "<div class='card'>";
  html += "<button type='button' class='relay-btn on' onclick='toggleAllRelays()'>Basculer tous les relais</button>";
  html += "</div>";
  
  // Section Entrées
  html += "<div class='card'>";
  html += "<h2>Entrees Digitales (8)</h2>";
  html += "<div class='grid4'>";
  for (int i = 0; i < 8; i++) {
    // inputStates[i] == true  => entrée ACTIVE (niveau bas, INPUT_PULLUP)
    bool isActive = inputStates[i];
    html += "<div id='input_";
    html += String(i + 1);
    html += "' class='input-item ";
    // Couleurs conservées: HIGH=bleu, LOW=jaune
    html += (isActive ? "low" : "high");
    html += "'><div class='input-num'>Entrée ";
    html += String(i+1);
    html += "</div><div class='input-status' id='input_status_";
    html += String(i + 1);
    html += "'>";
    html += (isActive ? "ACTIVE" : "INACTIVE");
    html += "</div></div>";
  }
  html += "</div></div>";

  // Section Configuration
  html += "<div class='card'>";
  html += "<h2>Configuration (IP + MQTT)</h2>";
  html += "<div class='cfg-row'>";
  html += "<div class='cfg-field'><label>IP statique</label><input id='cfg_ip' placeholder='";
  html += cfgStaticIpStr;
  html += "'></div>";
  html += "<div class='cfg-field'><label>Passerelle</label><input id='cfg_gw' placeholder='";
  html += cfgGatewayStr;
  html += "'></div>";
  html += "<div class='cfg-field'><label>Masque</label><input id='cfg_mask' placeholder='";
  html += cfgSubnetStr;
  html += "'></div>";
  html += "<div class='cfg-field'><label>DNS</label><input id='cfg_dns' placeholder='";
  html += cfgDnsStr;
  html += "'></div>";
  html += "<div class='cfg-field'><label>Broker MQTT (IP)</label><input id='cfg_mqtt_ip' placeholder='";
  html += mqttStr;
  html += "'></div>";
  html += "<div class='cfg-field'><label>Port MQTT</label><input id='cfg_mqtt_port' placeholder='";
  html += String(mqttPort);
  html += "'></div>";
  html += "<div class='cfg-field'><label>Utilisateur MQTT</label><input id='cfg_mqtt_user' placeholder='";
  html += String(mqttUser);
  html += "'></div>";
  html += "<div class='cfg-field'><label>Mot de passe MQTT</label><input id='cfg_mqtt_pass' type='password' placeholder='(laisser vide pour ne pas changer)'></div>";
  html += "</div>";
  html += "<div class='cfg-actions'>";
  html += "<button type='button' class='relay-btn on' onclick='saveConfig()'>Enregistrer</button>";
  html += "<span class='cfg-msg' id='cfg_msg'></span>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class='card status-bar'>";
  html += "Systeme operationnel | Ethernet W5500 | IP: <span id='sb_ip'>";
  html += ipStr;
  html += "</span> | MQTT: <span id='sb_mqtt'>";
  html += mqttState;
  html += "</span>";
  html += "</div>";
  
  html += "</div>";
  html += "<script>";
  html += "function callApi(path){return fetch(path,{cache:'no-store'}).then(function(r){return r.text();});}";
  html += "function toggleRelay(n){callApi('/relay?num='+n+'&action=toggle').then(function(){setTimeout(pollStatus,120);});}";
  html += "function toggleAllRelays(){callApi('/relay?action=all_toggle').then(function(){setTimeout(pollStatus,180);});}";
  html += "function getVal(id){var el=document.getElementById(id); if(!el) return ''; return (el.value||'').trim(); }";
  html += "function setMsg(t){var el=document.getElementById('cfg_msg'); if(el) el.textContent=t; }";

  html += "function setClass(el, onClass, offClass, isOn){ if(!el) return; el.classList.remove(onClass); el.classList.remove(offClass); el.classList.add(isOn?onClass:offClass); }";
  html += "function pollStatus(){fetch('/api/status',{cache:'no-store'}).then(function(r){return r.json();}).then(function(s){";
  html += "var tv=document.getElementById('temp_val'); if(tv) tv.textContent=(Number(s.t)||0).toFixed(1)+'°C';";
  html += "var hv=document.getElementById('hum_val'); if(hv) hv.textContent=(Number(s.h)||0).toFixed(1)+'%';";
  html += "var sm=document.getElementById('sys_mqtt'); if(sm) sm.textContent=(s.mqtt? 'CONNECTE':'DECONNECTE');";
  html += "var sbm=document.getElementById('sb_mqtt'); if(sbm) sbm.textContent=(s.mqtt? 'CONNECTE':'DECONNECTE');";
  html += "if(Array.isArray(s.r)){";
  html += "for(var i=0;i<Math.min(8,s.r.length);i++){var n=i+1; var isOn=!!s.r[i];";
  html += "var box=document.getElementById('relay_'+n); setClass(box,'on','off',isOn);";
  html += "var st=document.getElementById('relay_status_'+n); if(st) st.textContent=isOn?'ON':'OFF';";
  html += "var btn=document.getElementById('relay_btn_'+n); if(btn){ setClass(btn,'on','off',isOn); btn.textContent=isOn?'Toggle (actuellement ON)':'Toggle (actuellement OFF)'; }";
  html += "}}";
  html += "if(Array.isArray(s.i)){";
  html += "for(var j=0;j<Math.min(8,s.i.length);j++){var n2=j+1; var isActive=!!s.i[j];";
  html += "var ib=document.getElementById('input_'+n2); setClass(ib,'low','high',isActive);";
  html += "var ist=document.getElementById('input_status_'+n2); if(ist) ist.textContent=isActive?'ACTIVE':'INACTIVE';";
  html += "}}";
  html += "}).catch(function(){});}";
  html += "function loadConfig(){fetch('/api/config',{cache:'no-store'}).then(function(r){return r.json();}).then(function(c){";
  html += "var ip=document.getElementById('cfg_ip'); if(ip) ip.placeholder=c.static_ip||ip.placeholder;";
  html += "var gw=document.getElementById('cfg_gw'); if(gw) gw.placeholder=c.gateway||gw.placeholder;";
  html += "var mk=document.getElementById('cfg_mask'); if(mk) mk.placeholder=c.subnet||mk.placeholder;";
  html += "var dn=document.getElementById('cfg_dns'); if(dn) dn.placeholder=c.dns1||dn.placeholder;";
  html += "var mi=document.getElementById('cfg_mqtt_ip'); if(mi) mi.placeholder=c.broker_ip||mi.placeholder;";
  html += "var mp=document.getElementById('cfg_mqtt_port'); if(mp) mp.placeholder=String(c.broker_port||mp.placeholder);";
  html += "var mu=document.getElementById('cfg_mqtt_user'); if(mu) mu.placeholder=c.username||mu.placeholder;";
  html += "}).catch(function(){});}";
  html += "function saveConfig(){";
  html += "setMsg('Enregistrement...');";
  html += "var payload={};";
  html += "var ip=getVal('cfg_ip'); if(ip) payload.static_ip=ip;";
  html += "var gw=getVal('cfg_gw'); if(gw) payload.gateway=gw;";
  html += "var mk=getVal('cfg_mask'); if(mk) payload.subnet=mk;";
  html += "var dn=getVal('cfg_dns'); if(dn) payload.dns1=dn;";
  html += "var b=getVal('cfg_mqtt_ip'); if(b) payload.broker_ip=b;";
  html += "var p=getVal('cfg_mqtt_port'); if(p) payload.broker_port=parseInt(p,10);";
  html += "var u=getVal('cfg_mqtt_user'); if(u) payload.username=u;";
  html += "var pw=getVal('cfg_mqtt_pass'); if(pw) payload.password=pw;";
  html += "fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)}).then(function(r){return r.json();}).then(function(res){";
  html += "if(res && res.restart){setMsg('OK. Redemarrage...'); setTimeout(function(){ if(res.new_ip){location.href='http://'+res.new_ip+'/';} else {location.reload();}},1500);}";
  html += "else {setMsg('OK. MQTT mis a jour.'); setTimeout(function(){location.reload();},500);}";
  html += "}).catch(function(){setMsg('Erreur sauvegarde');});";
  html += "}";
  html += "loadConfig();";
  html += "pollStatus(); setInterval(pollStatus,1000);";
  html += "</script>";
  html += "<footer>API JSON: /api/status | Config: /api/config | Controle: /relay?num=N&action=toggle</footer>";
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
  
  // Parser commandes de relais: waveshare/relay/cmd
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
  // Reconnexion MQTT NON-BLOQUANTE (important pour garder HTTP réactif)
  if (mqttClient.connected()) {
    mqttConnected = true;
    return;
  }

  mqttConnected = false;

  unsigned long now = millis();
  if (now - lastMqttReconnectAttempt < mqttReconnectIntervalMs) {
    return;
  }
  lastMqttReconnectAttempt = now;

  Serial.print("Tentative MQTT... ");

  bool ok = false;
  if (mqttUser[0] == '\0') {
    ok = mqttClient.connect(mqttClientID);
  } else {
    ok = mqttClient.connect(mqttClientID, mqttUser, mqttPassword);
  }

  if (ok) {
    Serial.println("✓ Connecté");
    mqttConnected = true;
    mqttClient.subscribe(topicRelayCmd);
    Serial.println("✓ Souscrit à " + String(topicRelayCmd));
    mqttPublishStatus();
  } else {
    Serial.printf("✗ Erreur: code %d\n", mqttClient.state());
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

static void sendHttp(EthernetClient &client, const char *status, const char *contentType, const String &body) {
  client.print("HTTP/1.1 ");
  client.println(status);
  client.println("Connection: close");
  client.print("Content-Type: ");
  client.println(contentType);
  client.print("Content-Length: ");
  client.println(body.length());
  client.println();

  const char *ptr = body.c_str();
  size_t remaining = body.length();
  while (remaining > 0 && client.connected()) {
    size_t chunk = remaining;
    if (chunk > 1024) chunk = 1024;
    size_t written = client.write((const uint8_t *)ptr, chunk);
    if (written == 0) {
      delay(1);
      continue;
    }
    ptr += written;
    remaining -= written;
  }
  client.flush();
}

static String getQueryParam(const String &query, const char *key) {
  // query without leading '?'
  String k = String(key) + "=";
  int start = query.indexOf(k);
  if (start < 0) return "";
  start += k.length();
  int end = query.indexOf('&', start);
  if (end < 0) end = query.length();
  return query.substring(start, end);
}

static void handleRelayQuery(const String &query) {
  // Supported:
  //  /relay?num=1&action=toggle
  //  /relay?action=all_toggle
  String action = getQueryParam(query, "action");
  String numStr = getQueryParam(query, "num");

  if (action == "all_toggle") {
    for (int i = 0; i < 8; i++) setRelay(i, !relayStates[i]);
    return;
  }

  int relayNum = numStr.toInt();
  if (relayNum < 1 || relayNum > 8) return;
  int relayIndex = relayNum - 1;

  if (action == "toggle") {
    setRelay(relayIndex, !relayStates[relayIndex]);
  } else if (action == "on") {
    setRelay(relayIndex, true);
  } else if (action == "off") {
    setRelay(relayIndex, false);
  }
}

void setupWebServer() {
  webServer.begin(httpPort);
  Serial.println("✓ HTTP server started (W5500) port 80");
}

void handleHttpLoop() {
  EthernetClient client = webServer.available();
  if (!client) return;

  client.setTimeout(200);

  // Read request line (e.g. "GET /path?x=y HTTP/1.1")
  String requestLine = client.readStringUntil('\n');
  requestLine.trim();

  size_t contentLength = 0;
  String otaKeyHeader = "";
  // Read headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) break;
    if (line.startsWith("Content-Length:")) {
      contentLength = (size_t)line.substring(String("Content-Length:").length()).toInt();
    } else {
      String lower = line;
      lower.toLowerCase();
      if (lower.startsWith("x-ota-key:")) {
        otaKeyHeader = line.substring(String("X-OTA-Key:").length());
        otaKeyHeader.trim();
      }
    }
  }

  // Parse path
  String method;
  String target;
  int sp1 = requestLine.indexOf(' ');
  int sp2 = requestLine.indexOf(' ', sp1 + 1);
  if (sp1 > 0 && sp2 > sp1) {
    method = requestLine.substring(0, sp1);
    target = requestLine.substring(sp1 + 1, sp2);
  }

  String path = target;
  String query = "";
  int q = target.indexOf('?');
  if (q >= 0) {
    path = target.substring(0, q);
    query = target.substring(q + 1);
  }

#if ENABLE_OTA_HTTP
  if (method == "POST" && path == "/api/ota") {
    DynamicJsonDocument resp(256);

    if (otaKey[0] == '\0') {
      resp["ok"] = 0;
      resp["error"] = "ota_key_not_set";
      String out;
      serializeJson(resp, out);
      sendHttp(client, "403 Forbidden", "application/json", out);
      delay(5);
      client.stop();
      return;
    }

    if (otaKeyHeader.length() == 0 || otaKeyHeader != String(otaKey)) {
      resp["ok"] = 0;
      resp["error"] = "bad_ota_key";
      String out;
      serializeJson(resp, out);
      sendHttp(client, "401 Unauthorized", "application/json", out);
      delay(5);
      client.stop();
      return;
    }

    if (contentLength == 0) {
      resp["ok"] = 0;
      resp["error"] = "missing_content_length";
      String out;
      serializeJson(resp, out);
      sendHttp(client, "411 Length Required", "application/json", out);
      delay(5);
      client.stop();
      return;
    }

    Serial.printf("OTA: starting update (%u bytes)\n", (unsigned)contentLength);
    if (!Update.begin(contentLength)) {
      resp["ok"] = 0;
      resp["error"] = "update_begin_failed";
      resp["code"] = (int)Update.getError();
      String out;
      serializeJson(resp, out);
      sendHttp(client, "500 Internal Server Error", "application/json", out);
      delay(5);
      client.stop();
      return;
    }

    size_t received = 0;
    uint32_t lastData = millis();
    uint8_t buf[1024];
    while (received < contentLength && client.connected()) {
      int avail = client.available();
      if (avail > 0) {
        size_t toRead = (size_t)avail;
        if (toRead > sizeof(buf)) toRead = sizeof(buf);
        if (toRead > (contentLength - received)) toRead = (contentLength - received);
        int n = client.read(buf, (int)toRead);
        if (n > 0) {
          size_t w = Update.write(buf, (size_t)n);
          if (w != (size_t)n) {
            Update.abort();
            resp["ok"] = 0;
            resp["error"] = "update_write_failed";
            resp["code"] = (int)Update.getError();
            String out;
            serializeJson(resp, out);
            sendHttp(client, "500 Internal Server Error", "application/json", out);
            delay(5);
            client.stop();
            return;
          }
          received += (size_t)n;
          lastData = millis();
        }
      } else {
        if (millis() - lastData > 5000) break;
        delay(1);
      }
    }

    if (received != contentLength) {
      Update.abort();
      resp["ok"] = 0;
      resp["error"] = "incomplete_upload";
      resp["received"] = (unsigned)received;
      resp["expected"] = (unsigned)contentLength;
      String out;
      serializeJson(resp, out);
      sendHttp(client, "400 Bad Request", "application/json", out);
      delay(5);
      client.stop();
      return;
    }

    bool ok = Update.end(true);
    if (!ok) {
      resp["ok"] = 0;
      resp["error"] = "update_end_failed";
      resp["code"] = (int)Update.getError();
      String out;
      serializeJson(resp, out);
      sendHttp(client, "500 Internal Server Error", "application/json", out);
      delay(5);
      client.stop();
      return;
    }

    resp["ok"] = 1;
    resp["reboot"] = 1;
    String out;
    serializeJson(resp, out);
    sendHttp(client, "200 OK", "application/json", out);

    delay(250);
    ESP.restart();
    return;
  }
#endif

  String body = "";
  if (method == "POST" && contentLength > 0) {
    body.reserve(contentLength + 1);
    uint32_t start = millis();
    while (body.length() < contentLength && client.connected()) {
      if (client.available()) {
        body += (char)client.read();
        start = millis();
      } else {
        if (millis() - start > 500) break;
        delay(1);
      }
    }
  }

  // Routing
  if (method == "GET" && path == "/api/status") {
    DynamicJsonDocument doc(768);
    JsonArray r = doc.createNestedArray("r");
    JsonArray i = doc.createNestedArray("i");
    for (int k = 0; k < 8; k++) {
      r.add(relayStates[k] ? 1 : 0);
      i.add(inputStates[k] ? 1 : 0);
    }
    doc["t"] = temperature;
    doc["h"] = humidity;
    doc["mqtt"] = mqttConnected ? 1 : 0;
    doc["uptime_ms"] = millis();

    String body;
    serializeJson(doc, body);
    sendHttp(client, "200 OK", "application/json", body);
  } else if (method == "GET" && path == "/api/config") {
    DynamicJsonDocument doc(512);
    doc["static_ip"] = staticIP.toString();
    doc["gateway"] = gateway.toString();
    doc["subnet"] = subnet.toString();
    doc["dns1"] = dns1.toString();
    doc["broker_ip"] = mqttServer.toString();
    doc["broker_port"] = mqttPort;
    doc["username"] = mqttUser;
    doc["ota_key_set"] = (otaKey[0] != '\0') ? 1 : 0;
    doc["mqtt_connected"] = mqttConnected ? 1 : 0;
    String out;
    serializeJson(doc, out);
    sendHttp(client, "200 OK", "application/json", out);
  } else if (method == "POST" && path == "/api/config") {
    DynamicJsonDocument doc(512);
    DynamicJsonDocument resp(256);

    IPAddress oldIp = staticIP;
    IPAddress oldMqtt = mqttServer;
    uint16_t oldPort = mqttPort;

    DeserializationError err = deserializeJson(doc, body);
    if (err) {
      resp["ok"] = 0;
      resp["error"] = "bad_json";
      String out;
      serializeJson(resp, out);
      sendHttp(client, "400 Bad Request", "application/json", out);
    } else {
      if (doc.containsKey("static_ip")) {
        String ipStr = doc["static_ip"].as<String>();
        IPAddress parsed;
        uint32_t parts[4];
        if (sscanf(ipStr.c_str(), "%lu.%lu.%lu.%lu", &parts[0], &parts[1], &parts[2], &parts[3]) == 4) {
          parsed = IPAddress(parts[0], parts[1], parts[2], parts[3]);
          staticIP = parsed;
        }
      }
      if (doc.containsKey("gateway")) {
        String ipStr = doc["gateway"].as<String>();
        uint32_t parts[4];
        if (sscanf(ipStr.c_str(), "%lu.%lu.%lu.%lu", &parts[0], &parts[1], &parts[2], &parts[3]) == 4) {
          gateway = IPAddress(parts[0], parts[1], parts[2], parts[3]);
        }
      }
      if (doc.containsKey("subnet")) {
        String ipStr = doc["subnet"].as<String>();
        uint32_t parts[4];
        if (sscanf(ipStr.c_str(), "%lu.%lu.%lu.%lu", &parts[0], &parts[1], &parts[2], &parts[3]) == 4) {
          subnet = IPAddress(parts[0], parts[1], parts[2], parts[3]);
        }
      }
      if (doc.containsKey("dns1")) {
        String ipStr = doc["dns1"].as<String>();
        uint32_t parts[4];
        if (sscanf(ipStr.c_str(), "%lu.%lu.%lu.%lu", &parts[0], &parts[1], &parts[2], &parts[3]) == 4) {
          dns1 = IPAddress(parts[0], parts[1], parts[2], parts[3]);
        }
      }
      if (doc.containsKey("broker_ip")) {
        String ipStr = doc["broker_ip"].as<String>();
        uint32_t parts[4];
        if (sscanf(ipStr.c_str(), "%lu.%lu.%lu.%lu", &parts[0], &parts[1], &parts[2], &parts[3]) == 4) {
          mqttServer = IPAddress(parts[0], parts[1], parts[2], parts[3]);
        }
      }
      if (doc.containsKey("broker_port")) {
        mqttPort = (uint16_t)doc["broker_port"].as<int>();
      }
      if (doc.containsKey("username")) {
        strlcpy(mqttUser, doc["username"].as<const char*>(), sizeof(mqttUser));
      }
      if (doc.containsKey("password")) {
        const char *pw = doc["password"].as<const char*>();
        if (pw && strlen(pw) > 0) {
          strlcpy(mqttPassword, pw, sizeof(mqttPassword));
        }
      }

      if (doc.containsKey("ota_key")) {
        const char *k = doc["ota_key"].as<const char*>();
        if (k) {
          strlcpy(otaKey, k, sizeof(otaKey));
        }
      }

      saveMQTTConfig();

      bool needRestart = (oldIp != staticIP);
      resp["ok"] = 1;
      resp["restart"] = needRestart ? 1 : 0;
      if (needRestart) resp["new_ip"] = staticIP.toString();

      String out;
      serializeJson(resp, out);
      sendHttp(client, "200 OK", "application/json", out);

      if (!needRestart) {
        if (oldMqtt != mqttServer || oldPort != mqttPort) {
          mqttClient.setServer(mqttServer, mqttPort);
        }
        mqttClient.disconnect();
        mqttReconnect();
      }

      if (needRestart) {
        delay(250);
        ESP.restart();
      }
    }
  } else if (method == "GET" && path == "/relay") {
    handleRelayQuery(query);
    sendHttp(client, "200 OK", "text/plain", "OK");
  } else if (method == "GET" && path == "/") {
    // Render snapshot HTML (auto-refresh like in docs)
    String page = getHtmlPage();
    sendHttp(client, "200 OK", "text/html; charset=utf-8", page);
  } else {
    sendHttp(client, "404 Not Found", "text/plain", "Not Found");
  }

  delay(5);
  client.stop();
}


void setup() {
  Serial.begin(9600);
  delay(3000);  // Extra long delay for monitor to connect
  
  Serial.write(0x00); Serial.write(0xFF);  // Flush
  delay(500);

  // Initialisation SPIFFS et chargement config AVANT Ethernet.begin (pour appliquer l'IP)
  initSPIFFS();
  loadMQTTConfig();
  Serial.println("\n\n╔════════════════════════════════════════╗");
  Serial.println("║ === DÉMARRAGE ESP32-S3-ETH-8DI-8RO ═══ ║");
  Serial.println("╚════════════════════════════════════════╝");
  delay(500);
  
  // Initialisation I2C pour TCA9554
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN); // SDA=42, SCL=41 (Waveshare official)
  Wire.setClock(100000);
  Serial.println("✓ I2C initialisé (SDA=42, SCL=41)");
  
  // Configuration TCA9554 @ 0x20
  // Registres TCA9554: 0x00 Input, 0x01 Output, 0x02 Polarity, 0x03 Configuration
  // Relais: logique active HIGH (bit HIGH = relais ON)
  delay(100);

  // Polarity: pas d'inversion
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(0x02);
  Wire.write(0x00);
  Wire.endTransmission();

  // Configuration: tous en OUTPUT
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(0x03);
  Wire.write(0x00);
  Wire.endTransmission();

  // Outputs: tous LOW => relais OFF
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(0x01);
  Wire.write(0x00);
  Wire.endTransmission();

  for (int i = 0; i < 8; i++) relayStates[i] = false;
  Serial.println("✓ TCA9554 configuré (8 relais OFF au démarrage)");
  
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
  
  // Configuration HTTP Web Server
  setupWebServer();
  
  // Configuration MQTT
  setupMqtt();
  
  Serial.println("\n=== Système prêt ===\n");
  Serial.println("Accès interface web: http://");
  Serial.print(Ethernet.localIP());
  Serial.println("/");
}

void loop() {
  // Maintenir la connexion Ethernet
  Ethernet.maintain();
  
  // Gestion HTTP Web Server
  handleHttpLoop();
  
  // Gestion MQTT
  if (!mqttClient.connected()) {
    mqttReconnect();
  } else {
    mqttClient.loop();  // Process incoming messages
    
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
