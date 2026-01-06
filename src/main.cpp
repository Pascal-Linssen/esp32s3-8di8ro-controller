#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <DHT.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <SPIFFS.h>
#include <Update.h>
#include <stdarg.h>
#include <stdio.h>
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
// ========================= MQTT IDENTIFIANTS =========================
// Identifiants MQTT utilisés par le firmware:
// - Fallback "en dur": DEFAULT_MQTT_USER / DEFAULT_MQTT_PASSWORD (ci-dessous)
// - Si SPIFFS est disponible, ces valeurs peuvent être SURCHARGÉES depuis /config.json
//   via loadMQTTConfig() dans src/web_config.h (configurable depuis l'interface Web /api/config).
//
// NOTE: mettre des identifiants en dur n'est pas recommandé (ne pas commit sur Git).
#ifndef DEFAULT_MQTT_USER
#define DEFAULT_MQTT_USER ""
#endif
#ifndef DEFAULT_MQTT_PASSWORD
#define DEFAULT_MQTT_PASSWORD ""
#endif
char mqttUser[50] = DEFAULT_MQTT_USER;
char mqttPassword[50] = DEFAULT_MQTT_PASSWORD;
// ====================================================================
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

// Labels I/O (utilisés pour la publication MQTT "_named" et via /api/config)
char relayLabels[8][16] = {"relay1", "relay2", "relay3", "relay4", "relay5", "relay6", "relay7", "relay8"};
char inputLabels[8][16] = {"input1", "input2", "input3", "input4", "input5", "input6", "input7", "input8"};

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

// SPIFFS status (défini ici, utilisé dans web_config.h)
bool spiffsReady = false;

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

// Suivi du lien Ethernet (W5500)
int lastEthLinkStatus = -1;
unsigned long lastEthLinkCheck = 0;
const unsigned long ethLinkCheckIntervalMs = 1000;

// ===== FONCTIONS FORWARD =====
void setRelay(int relay, bool state);
void readInputs();
void readSensors();
String getHtmlPage();
void handleHttpConnections();

// ===== LOGS HTTP À DISTANCE (sans USB / sans MQTT) =====
// Buffer circulaire en RAM, lisible via GET /api/logs
static const size_t LOG_RING_LINES = 80;
static String logRing[LOG_RING_LINES];
static size_t logRingHead = 0;
static size_t logRingCount = 0;

static void logRingAddLine(const String &line) {
  String s = line;
  s.replace("\r", "");
  // Limiter une ligne pour éviter l'explosion RAM
  if (s.length() > 240) s = s.substring(0, 240) + "...";
  logRing[logRingHead] = s;
  logRingHead = (logRingHead + 1) % LOG_RING_LINES;
  if (logRingCount < LOG_RING_LINES) logRingCount++;
}

static void logLine(const String &line) {
  Serial.println(line);
  logRingAddLine(line);
}

static void logLinef(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.println(buf);
  logRingAddLine(String(buf));
}
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
  html += "@media (max-width:900px){.grid2{grid-template-columns:1fr;}.grid4{grid-template-columns:repeat(2,1fr);}.cfg-row{grid-template-columns:1fr;}}";
  html += "@media (max-width:520px){.grid4{grid-template-columns:1fr;}}";
  html += ".stat {text-align:center;padding:15px;background:#0b0f0d;border-radius:5px;border:1px solid #1c5a41;}";
  html += ".stat-value {font-size:24px;font-weight:bold;color:#7ef7c8;}";
  html += ".stat-label {font-size:12px;color:#b7d7c8;margin-top:5px;}";
  html += ".relay-item {padding:15px;border:2px solid #1c5a41;border-radius:5px;text-align:center;transition:all 0.2s;}";
  html += ".relay-item:hover {border-color:#2ea87f;background:#0b0f0d;}";
  html += ".relay-item.on {background:#0c1f18;border-color:#2ea87f;}";
  html += ".relay-item.off {background:#0e1311;border-color:#2b3a33;}";
  html += ".relay-num {font-size:14px;color:#b7d7c8;margin-bottom:10px;}";
  html += ".io-label {display:block;margin-top:6px;font-size:12px;color:#7ef7c8;word-break:break-word;}";
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
    html += "<span class='io-label' id='relay_label_";
    html += String(i + 1);
    html += "'>";
    html += relayLabels[i];
    html += "</span></div><div class='relay-status' id='relay_status_";
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
    html += "<span class='io-label' id='input_label_";
    html += String(i + 1);
    html += "'>";
    html += inputLabels[i];
    html += "</span></div><div class='input-status' id='input_status_";
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

  // Section Noms I/O
  html += "<div class='card'>";
  html += "<h2>Noms des entrees / sorties</h2>";
  html += "<div class='cfg-row'>";
  html += "<div>";
  for (int i = 0; i < 8; i++) {
    html += "<div class='cfg-field'><label>Relais ";
    html += String(i + 1);
    html += "</label><input id='cfg_rl_";
    html += String(i + 1);
    html += "' maxlength='15' placeholder='";
    html += relayLabels[i];
    html += "'></div>";
  }
  html += "</div>";
  html += "<div>";
  for (int i = 0; i < 8; i++) {
    html += "<div class='cfg-field'><label>Entree ";
    html += String(i + 1);
    html += "</label><input id='cfg_il_";
    html += String(i + 1);
    html += "' maxlength='15' placeholder='";
    html += inputLabels[i];
    html += "'></div>";
  }
  html += "</div>";
  html += "</div>";
  html += "<div class='cfg-actions'>";
  html += "<button type='button' class='relay-btn on' onclick='saveConfig()'>Enregistrer</button>";
  html += "<span class='cfg-msg'>Sauvegarde aussi IP/MQTT si modifies</span>";
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
  html += "var pollInFlight=false;";
  html += "var last={t:null,h:null,mqtt:null,r:null,i:null};";
  html += "function callApi(path){return fetch(path,{cache:'no-store'}).then(function(r){return r.text();});}";
  html += "function toggleRelay(n){callApi('/relay?num='+n+'&action=toggle').then(function(){setTimeout(pollStatus,150);});}";
  html += "function toggleAllRelays(){callApi('/relay?action=all_toggle').then(function(){setTimeout(pollStatus,220);});}";
  html += "function getVal(id){var el=document.getElementById(id); if(!el) return ''; return (el.value||'').trim(); }";
  html += "function setMsg(t){var el=document.getElementById('cfg_msg'); if(el && el.textContent!==t) el.textContent=t; }";
  html += "function setText(id, txt){var el=document.getElementById(id); if(el && el.textContent!==txt) el.textContent=txt; }";
  html += "function setVal(id, txt){var el=document.getElementById(id); if(el && el.value!==txt) el.value=txt; }";
  html += "function normLabel(s){s=(s||'').trim(); if(s.length>15) s=s.substring(0,15); return s;}";
  html += "function getLabelFromInput(inputId, spanId, fallback){var el=document.getElementById(inputId); var v=el?normLabel(el.value):''; if(!v){var sp=document.getElementById(spanId); v=sp?normLabel(sp.textContent):'';} if(!v) v=fallback; if(el && el.value!==v) el.value=v; return v;}";
  html += "function setClass(el, onClass, offClass, isOn){ if(!el) return; var want=isOn?onClass:offClass; if(el.classList.contains(want)) return; el.classList.remove(onClass); el.classList.remove(offClass); el.classList.add(want); }";
  html += "function sameArr(a,b){ if(!Array.isArray(a)||!Array.isArray(b)||a.length!==b.length) return false; for(var k=0;k<a.length;k++){ if(!!a[k]!==!!b[k]) return false; } return true; }";
  html += "function pollStatus(){ if(pollInFlight) return; pollInFlight=true; fetch('/api/status',{cache:'no-store'}).then(function(r){return r.json();}).then(function(s){";
  html += "var tStr=(Number(s.t)||0).toFixed(1)+'°C'; if(last.t!==tStr){ last.t=tStr; setText('temp_val',tStr);}";
  html += "var hStr=(Number(s.h)||0).toFixed(1)+'%'; if(last.h!==hStr){ last.h=hStr; setText('hum_val',hStr);}";
  html += "var mqttStr=(s.mqtt? 'CONNECTE':'DECONNECTE'); if(last.mqtt!==mqttStr){ last.mqtt=mqttStr; setText('sys_mqtt',mqttStr); setText('sb_mqtt',mqttStr);}";
  html += "if(Array.isArray(s.r) && !sameArr(last.r,s.r)){ last.r=s.r.slice(0,8); for(var i=0;i<Math.min(8,s.r.length);i++){var n=i+1; var isOn=!!s.r[i]; var box=document.getElementById('relay_'+n); setClass(box,'on','off',isOn); setText('relay_status_'+n, isOn?'ON':'OFF'); var btn=document.getElementById('relay_btn_'+n); if(btn){ setClass(btn,'on','off',isOn); var bt=isOn?'Toggle (actuellement ON)':'Toggle (actuellement OFF)'; if(btn.textContent!==bt) btn.textContent=bt; } } }";
  html += "if(Array.isArray(s.i) && !sameArr(last.i,s.i)){ last.i=s.i.slice(0,8); for(var j=0;j<Math.min(8,s.i.length);j++){var n2=j+1; var isActive=!!s.i[j]; var ib=document.getElementById('input_'+n2); setClass(ib,'low','high',isActive); setText('input_status_'+n2, isActive?'ACTIVE':'INACTIVE'); } }";
  html += "}).catch(function(){}).finally(function(){pollInFlight=false;});}";
  html += "function loadConfig(){fetch('/api/config',{cache:'no-store'}).then(function(r){return r.json();}).then(function(c){";
  html += "var ip=document.getElementById('cfg_ip'); if(ip) ip.placeholder=c.static_ip||ip.placeholder;";
  html += "var gw=document.getElementById('cfg_gw'); if(gw) gw.placeholder=c.gateway||gw.placeholder;";
  html += "var mk=document.getElementById('cfg_mask'); if(mk) mk.placeholder=c.subnet||mk.placeholder;";
  html += "var dn=document.getElementById('cfg_dns'); if(dn) dn.placeholder=c.dns1||dn.placeholder;";
  html += "var mi=document.getElementById('cfg_mqtt_ip'); if(mi) mi.placeholder=c.broker_ip||mi.placeholder;";
  html += "var mp=document.getElementById('cfg_mqtt_port'); if(mp) mp.placeholder=String(c.broker_port||mp.placeholder);";
  html += "var mu=document.getElementById('cfg_mqtt_user'); if(mu) mu.placeholder=c.username||mu.placeholder;";
  html += "if(Array.isArray(c.relay_labels)){for(var a=0;a<Math.min(8,c.relay_labels.length);a++){var n=a+1; var v=normLabel(c.relay_labels[a]); if(v){setText('relay_label_'+n,v); setVal('cfg_rl_'+n,v);} }}";
  html += "if(Array.isArray(c.input_labels)){for(var b=0;b<Math.min(8,c.input_labels.length);b++){var n2=b+1; var v2=normLabel(c.input_labels[b]); if(v2){setText('input_label_'+n2,v2); setVal('cfg_il_'+n2,v2);} }}";
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
  html += "var rlbl=[]; var ilbl=[]; for(var x=1;x<=8;x++){rlbl.push(getLabelFromInput('cfg_rl_'+x,'relay_label_'+x,'relay'+x)); ilbl.push(getLabelFromInput('cfg_il_'+x,'input_label_'+x,'input'+x));} payload.relay_labels=rlbl; payload.input_labels=ilbl;";
  html += "fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)}).then(function(r){return r.json();}).then(function(res){";
  html += "if(!res || res.ok!==1){setMsg('Erreur sauvegarde: '+((res&&res.error)?res.error:'inconnue')); return;}";
  html += "if(res && res.restart){setMsg('OK. Redemarrage...'); setTimeout(function(){ if(res.new_ip){location.href='http://'+res.new_ip+'/';} else {location.reload();}},1500);}";
  html += "else {setMsg('OK. Sauvegarde faite.'); var pwEl=document.getElementById('cfg_mqtt_pass'); if(pwEl) pwEl.value=''; loadConfig(); setTimeout(pollStatus,200);}";
  html += "}).catch(function(){setMsg('Erreur sauvegarde');});";
  html += "}";
  html += "loadConfig();";
  html += "pollStatus(); setInterval(pollStatus,1500);";
  html += "</script>";
  html += "<footer>API JSON: /api/status | Config: /api/config | Controle: /relay?num=N&action=toggle | MQTT: waveshare/relay/status_named & waveshare/input/status_named</footer>";
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

  // État des relais "nommé" (clé = label)
  StaticJsonDocument<384> relayNamedDoc;
  for (int i = 0; i < 8; i++) {
    relayNamedDoc[relayLabels[i]] = relayStates[i] ? 1 : 0;
  }
  String relayNamed;
  serializeJson(relayNamedDoc, relayNamed);
  mqttClient.publish("waveshare/relay/status_named", relayNamed.c_str());
  
  // État des entrées
  StaticJsonDocument<256> inputDoc;
  for (int i = 0; i < 8; i++) {
    inputDoc[i] = inputStates[i] ? 1 : 0;
  }
  String inputStatus;
  serializeJson(inputDoc, inputStatus);
  mqttClient.publish(topicInputStatus, inputStatus.c_str());

  // État des entrées "nommé" (clé = label)
  StaticJsonDocument<384> inputNamedDoc;
  for (int i = 0; i < 8; i++) {
    inputNamedDoc[inputLabels[i]] = inputStates[i] ? 1 : 0;
  }
  String inputNamed;
  serializeJson(inputNamedDoc, inputNamed);
  mqttClient.publish("waveshare/input/status_named", inputNamed.c_str());
  
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

  // Ne pas spammer des tentatives si le lien Ethernet est down
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    return;
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    return;
  }

  unsigned long now = millis();
  if (now - lastMqttReconnectAttempt < mqttReconnectIntervalMs) {
    return;
  }
  lastMqttReconnectAttempt = now;

  Serial.print("Tentative MQTT... ");

  // Assainir le socket avant de retenter (utile après coupure de câble / reset)
  ethClient.stop();

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
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(2);
  
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
  bool isChunked = false;
  // Read headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) break;
    String lower = line;
    lower.toLowerCase();
    if (lower.startsWith("content-length:")) {
      int colon = line.indexOf(':');
      if (colon >= 0) {
        String v = line.substring(colon + 1);
        v.trim();
        contentLength = (size_t)v.toInt();
      }
    } else if (lower.startsWith("transfer-encoding:")) {
      if (lower.indexOf("chunked") >= 0) {
        isChunked = true;
      }
    } else if (lower.startsWith("x-ota-key:")) {
      int colon = line.indexOf(':');
      otaKeyHeader = (colon >= 0) ? line.substring(colon + 1) : "";
      otaKeyHeader.trim();
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
  if (method == "POST") {
    if (contentLength > 0) {
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
    } else if (isChunked && path == "/api/config") {
      // Décodage minimal du chunked encoding (suffisant pour le JSON de config)
      const size_t MAX_BODY = 2048;
      uint32_t start = millis();
      while (client.connected()) {
        if (millis() - start > 2000) break;
        String sizeLine = client.readStringUntil('\n');
        sizeLine.trim();
        if (sizeLine.length() == 0) continue;
        char *endptr = nullptr;
        unsigned long chunkSize = strtoul(sizeLine.c_str(), &endptr, 16);
        if (chunkSize == 0) {
          // Consommer la ligne vide finale si présente
          client.readStringUntil('\n');
          break;
        }
        while (client.connected() && client.available() < (int)chunkSize) {
          if (millis() - start > 2000) break;
          delay(1);
        }
        for (unsigned long i = 0; i < chunkSize && client.connected(); i++) {
          int c = client.read();
          if (c < 0) break;
          if (body.length() < MAX_BODY) body += (char)c;
        }
        // Consommer CRLF après le chunk
        client.readStringUntil('\n');
        start = millis();
      }
    }
  }

  auto sanitizeJsonForLog = [](String s) -> String {
    // Masquer le mot de passe si présent: "password":"..."
    int keyPos = s.indexOf("\"password\"");
    if (keyPos < 0) return s;
    int colon = s.indexOf(':', keyPos);
    if (colon < 0) return s;
    int firstQuote = s.indexOf('"', colon);
    if (firstQuote < 0) return s;
    int secondQuote = s.indexOf('"', firstQuote + 1);
    if (secondQuote < 0) return s;
    String prefix = s.substring(0, firstQuote + 1);
    String suffix = s.substring(secondQuote);
    return prefix + "***" + suffix;
  };

  // Routing
  if (method == "GET" && path == "/api/logs") {
    // Retourne les dernières lignes de log (buffer circulaire RAM)
    String out;
    out.reserve(logRingCount * 64);
    size_t start = (logRingCount == LOG_RING_LINES) ? logRingHead : 0;
    for (size_t i = 0; i < logRingCount; i++) {
      size_t idx = (start + i) % LOG_RING_LINES;
      if (logRing[idx].length() == 0) continue;
      out += logRing[idx];
      out += "\n";
    }
    sendHttp(client, "200 OK", "text/plain; charset=utf-8", out);
  } else if (method == "GET" && path == "/api/config_raw") {
    // Diagnostic: renvoie le contenu brut de /config.json (si présent)
    if (!spiffsReady || !SPIFFS.exists(CONFIG_FILE)) {
      sendHttp(client, "404 Not Found", "text/plain; charset=utf-8", "config_not_found");
    } else {
      File f = SPIFFS.open(CONFIG_FILE, "r");
      if (!f) {
        sendHttp(client, "500 Internal Server Error", "text/plain; charset=utf-8", "config_open_failed");
      } else {
        String content = f.readString();
        f.close();
        sendHttp(client, "200 OK", "application/json", content);
      }
    }
  } else if (method == "GET" && path == "/api/status") {
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
    doc["ip"] = Ethernet.localIP().toString();
    doc["ip_cfg"] = staticIP.toString();
    doc["gw_cfg"] = gateway.toString();
    doc["subnet_cfg"] = subnet.toString();
    doc["dns1_cfg"] = dns1.toString();

    String body;
    serializeJson(doc, body);
    sendHttp(client, "200 OK", "application/json", body);
  } else if (method == "GET" && path == "/api/config") {
    DynamicJsonDocument doc(1024);
    doc["static_ip"] = staticIP.toString();
    doc["gateway"] = gateway.toString();
    doc["subnet"] = subnet.toString();
    doc["dns1"] = dns1.toString();
    doc["broker_ip"] = mqttServer.toString();
    doc["broker_port"] = mqttPort;
    doc["username"] = mqttUser;
    doc["ota_key_set"] = (otaKey[0] != '\0') ? 1 : 0;
    doc["mqtt_connected"] = mqttConnected ? 1 : 0;

    JsonArray rlbl = doc.createNestedArray("relay_labels");
    JsonArray ilbl = doc.createNestedArray("input_labels");
    for (int i = 0; i < 8; i++) {
      rlbl.add(relayLabels[i]);
      ilbl.add(inputLabels[i]);
    }

    String out;
    serializeJson(doc, out);
    sendHttp(client, "200 OK", "application/json", out);
  } else if (method == "POST" && path == "/api/config") {
    DynamicJsonDocument doc(1024);
    DynamicJsonDocument resp(256);

    logLine("\n[HTTP] POST /api/config");
    logLinef("  Content-Length: %u", (unsigned)contentLength);
    logLinef("  Body length: %u", (unsigned)body.length());
    if (body.length() > 0) {
      String preview = body;
      if (preview.length() > 512) preview = preview.substring(0, 512) + "...";
      logLinef("  Body preview: %s", sanitizeJsonForLog(preview).c_str());
    }

    IPAddress oldIp = staticIP;
    IPAddress oldGw = gateway;
    IPAddress oldMask = subnet;
    IPAddress oldDns = dns1;
    IPAddress oldMqtt = mqttServer;
    uint16_t oldPort = mqttPort;

    DeserializationError err = deserializeJson(doc, body);
    if (err) {
      logLinef("  JSON error: %s", err.c_str());
      resp["ok"] = 0;
      resp["error"] = "bad_json";
      resp["body_len"] = (unsigned)body.length();
      resp["content_len"] = (unsigned)contentLength;
      String out;
      serializeJson(resp, out);
      sendHttp(client, "400 Bad Request", "application/json", out);
    } else {
      logLine("  JSON OK -> applying fields");
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

      // Mise à jour des labels (optionnel)
      if (doc.containsKey("relay_labels") && doc["relay_labels"].is<JsonArray>()) {
        JsonArray arr = doc["relay_labels"].as<JsonArray>();
        for (int i = 0; i < 8 && i < (int)arr.size(); i++) {
          const char *v = arr[i] | "";
          if (v && strlen(v) > 0) strlcpy(relayLabels[i], v, sizeof(relayLabels[i]));
        }
      }
      if (doc.containsKey("input_labels") && doc["input_labels"].is<JsonArray>()) {
        JsonArray arr = doc["input_labels"].as<JsonArray>();
        for (int i = 0; i < 8 && i < (int)arr.size(); i++) {
          const char *v = arr[i] | "";
          if (v && strlen(v) > 0) strlcpy(inputLabels[i], v, sizeof(inputLabels[i]));
        }
      }

      if (!saveMQTTConfig()) {
        resp["ok"] = 0;
        resp["error"] = "spiffs_save_failed";
        String out;
        serializeJson(resp, out);
        sendHttp(client, "500 Internal Server Error", "application/json", out);
        delay(5);
        client.stop();
        return;
      }

      logLinef("  Saved. IP=%s GW=%s MQTT=%s:%u user=%s pass_set=%s",
               staticIP.toString().c_str(),
               gateway.toString().c_str(),
               mqttServer.toString().c_str(),
               (unsigned)mqttPort,
               mqttUser,
               (mqttPassword[0] != '\0') ? "YES" : "NO");

      bool needRestart = (oldIp != staticIP) || (oldGw != gateway) || (oldMask != subnet) || (oldDns != dns1);
      resp["ok"] = 1;
      resp["restart"] = needRestart ? 1 : 0;
      if (needRestart) resp["new_ip"] = staticIP.toString();
      resp["current_ip"] = Ethernet.localIP().toString();
      resp["desired_ip"] = staticIP.toString();

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
  Serial.printf("MQTT user (boot): %s\n", mqttUser);
  Serial.printf("MQTT pass set (boot): %s\n", (mqttPassword[0] != '\0') ? "YES" : "NO");
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

  lastEthLinkStatus = Ethernet.linkStatus();
  
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

  // Surveiller le lien Ethernet et déclencher une reconnexion MQTT si besoin
  unsigned long now = millis();
  if (now - lastEthLinkCheck >= ethLinkCheckIntervalMs) {
    lastEthLinkCheck = now;
    int link = Ethernet.linkStatus();
    if (link != lastEthLinkStatus) {
      lastEthLinkStatus = link;
      if (link == LinkOFF) {
        Serial.println("⚠️ Ethernet link OFF -> MQTT disconnect");
        mqttConnected = false;
        mqttClient.disconnect();
        ethClient.stop();
      } else if (link == LinkON) {
        Serial.println("✓ Ethernet link ON -> MQTT reconnect pending");
        mqttConnected = false;
        mqttClient.disconnect();
        ethClient.stop();
        lastMqttReconnectAttempt = 0;
      } else {
        Serial.println("⚠️ Ethernet link status unknown");
      }
    }
  }
  
  // Gestion HTTP Web Server
  handleHttpLoop();
  
  // Gestion MQTT
  if (!mqttClient.connected()) {
    mqttReconnect();
  } else {
    // loop() retourne false quand la connexion est perdue; on force disconnect pour relancer mqttReconnect()
    if (!mqttClient.loop()) {
      mqttConnected = false;
      mqttClient.disconnect();
      ethClient.stop();
    } else {
      mqttConnected = true;
      // Publier l'état régulièrement
      if (millis() - lastMqttPublish >= mqttPublishInterval) {
        lastMqttPublish = millis();
        mqttPublishStatus();
      }
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
