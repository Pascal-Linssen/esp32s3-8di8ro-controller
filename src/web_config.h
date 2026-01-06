#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#ifndef SPIFFS_AUTO_FORMAT_ONCE
#define SPIFFS_AUTO_FORMAT_ONCE 0
#endif

// Variables globales (déclarées dans main.cpp)
extern IPAddress staticIP;
extern IPAddress gateway;
extern IPAddress subnet;
extern IPAddress dns1;

extern IPAddress mqttServer;
extern uint16_t mqttPort;
extern char mqttUser[50];
extern char mqttPassword[50];
extern char otaKey[64];
extern char topicRelayCmd[100];
extern char topicRelayStatus[100];
extern char topicInputStatus[100];
extern char topicSensorStatus[100];
extern char topicSystemStatus[100];
extern char relayLabels[8][16];
extern char inputLabels[8][16];
extern const char* CONFIG_FILE;

// SPIFFS status (défini dans main.cpp)
extern bool spiffsReady;

// ===== GESTION SPIFFS =====

void initSPIFFS() {
  spiffsReady = false;

  static Preferences prefs;
  prefs.begin("fs", false);
  bool formattedOnce = prefs.getBool("spiffs_fmt", false);

  // 1) Tentative sans format (préserve /config.json)
  if (SPIFFS.begin(false)) {
    spiffsReady = true;
    Serial.println("✓ SPIFFS ready");
    prefs.end();
    return;
  }

  // 2) Fallback: ne formater qu'une seule fois (sinon on efface /config.json à chaque boot).
  if (SPIFFS_AUTO_FORMAT_ONCE) {
    if (!formattedOnce) {
      Serial.println("⚠️ SPIFFS mount failed, formatting once...");
      if (SPIFFS.begin(true)) {
        spiffsReady = true;
        prefs.putBool("spiffs_fmt", true);
        Serial.println("✓ SPIFFS formatted + ready");
        prefs.end();
        return;
      }
    } else {
      Serial.println("✗ SPIFFS mount failed (format skipped: already formatted once)");
    }
  } else {
    Serial.println("✗ SPIFFS mount failed (auto-format disabled to preserve /config.json)");
  }

  Serial.println("✗ SPIFFS init failed");
  prefs.end();
}

static bool parseIpString(const String &ip, IPAddress &out) {
  uint32_t parts[4];
  if (sscanf(ip.c_str(), "%lu.%lu.%lu.%lu", &parts[0], &parts[1], &parts[2], &parts[3]) != 4) return false;
  out = IPAddress(parts[0], parts[1], parts[2], parts[3]);
  return true;
}

static void migrateTopicPrefix(char *topic, size_t topicSize, const char *oldPrefix, const char *newPrefix) {
  if (!topic || topicSize == 0 || !oldPrefix || !newPrefix) return;
  size_t oldLen = strlen(oldPrefix);
  if (oldLen == 0) return;
  if (strncmp(topic, oldPrefix, oldLen) != 0) return;

  String migrated = String(newPrefix) + String(topic + oldLen);
  strlcpy(topic, migrated.c_str(), topicSize);
}

void loadMQTTConfig() {
  if (!spiffsReady) {
    Serial.println("⚠️ SPIFFS not ready -> using defaults");
    return;
  }
  if (!SPIFFS.exists(CONFIG_FILE)) {
    Serial.println("⚠️  Config file not found, using defaults");
    return;
  }
  
  File file = SPIFFS.open(CONFIG_FILE, "r");
  if (!file) {
    Serial.println("✗ Failed to open config file for read");
    return;
  }
  DynamicJsonDocument doc(1536);
  
  if (deserializeJson(doc, file) == DeserializationError::Ok) {
    // Réseau (optionnel)
    {
      String ip = doc["static_ip"] | "";
      if (ip.length()) parseIpString(ip, staticIP);
      String gw = doc["gateway"] | "";
      if (gw.length()) parseIpString(gw, gateway);
      String mask = doc["subnet"] | "";
      if (mask.length()) parseIpString(mask, subnet);
      String dns = doc["dns1"] | "";
      if (dns.length()) parseIpString(dns, dns1);
    }

    String ip = doc["broker_ip"] | "192.168.1.200";
    parseIpString(ip, mqttServer);
    
    mqttPort = doc["broker_port"] | 1883;

    // IMPORTANT: ne pas écraser les valeurs existantes (fallback hardcodé) par des chaînes vides.
    if (doc.containsKey("username")) {
      String u = doc["username"].as<String>();
      if (u.length() > 0) {
        strlcpy(mqttUser, u.c_str(), sizeof(mqttUser));
      }
    }
    if (doc.containsKey("password")) {
      String p = doc["password"].as<String>();
      if (p.length() > 0) {
        strlcpy(mqttPassword, p.c_str(), sizeof(mqttPassword));
      }
    }
    strlcpy(otaKey, doc["ota_key"] | "", sizeof(otaKey));
    
    strlcpy(topicRelayCmd, doc["topic_relay_cmd"] | "waveshare/relay/cmd", sizeof(topicRelayCmd));
    strlcpy(topicRelayStatus, doc["topic_relay_status"] | "waveshare/relay/status", sizeof(topicRelayStatus));
    strlcpy(topicInputStatus, doc["topic_input_status"] | "waveshare/input/status", sizeof(topicInputStatus));
    strlcpy(topicSensorStatus, doc["topic_sensor_status"] | "waveshare/sensor/status", sizeof(topicSensorStatus));
    strlcpy(topicSystemStatus, doc["topic_system_status"] | "waveshare/system/status", sizeof(topicSystemStatus));

    // Labels I/O (optionnel)
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

    // Migration automatique: ancien préfixe -> nouveau
    migrateTopicPrefix(topicRelayCmd, sizeof(topicRelayCmd), "home/esp32/", "waveshare/");
    migrateTopicPrefix(topicRelayStatus, sizeof(topicRelayStatus), "home/esp32/", "waveshare/");
    migrateTopicPrefix(topicInputStatus, sizeof(topicInputStatus), "home/esp32/", "waveshare/");
    migrateTopicPrefix(topicSensorStatus, sizeof(topicSensorStatus), "home/esp32/", "waveshare/");
    migrateTopicPrefix(topicSystemStatus, sizeof(topicSystemStatus), "home/esp32/", "waveshare/");
    
    Serial.println("✓ MQTT config loaded from SPIFFS");
    Serial.printf("  MQTT user: %s\n", mqttUser);
  }
  
  file.close();
}

bool saveMQTTConfig() {
  if (!spiffsReady) {
    Serial.println("✗ SPIFFS not ready -> cannot save config");
    return false;
  }
  DynamicJsonDocument doc(1536);
  
  // Réseau
  doc["static_ip"] = staticIP.toString();
  doc["gateway"] = gateway.toString();
  doc["subnet"] = subnet.toString();
  doc["dns1"] = dns1.toString();
  
  // MQTT
  doc["broker_ip"] = mqttServer.toString();
  doc["broker_port"] = mqttPort;
  doc["username"] = mqttUser;
  doc["password"] = mqttPassword;
  doc["ota_key"] = otaKey;
  doc["topic_relay_cmd"] = topicRelayCmd;
  doc["topic_relay_status"] = topicRelayStatus;
  doc["topic_input_status"] = topicInputStatus;
  doc["topic_sensor_status"] = topicSensorStatus;
  doc["topic_system_status"] = topicSystemStatus;

  // Labels I/O
  JsonArray rlbl = doc.createNestedArray("relay_labels");
  JsonArray ilbl = doc.createNestedArray("input_labels");
  for (int i = 0; i < 8; i++) {
    rlbl.add(relayLabels[i]);
    ilbl.add(inputLabels[i]);
  }
  
  File file = SPIFFS.open(CONFIG_FILE, "w");
  if (!file) {
    Serial.println("✗ Failed to open config file for write");
    return false;
  }
  if (serializeJson(doc, file) == 0) {
    Serial.println("✗ Failed to write config JSON");
    file.close();
    return false;
  }
  file.close();

  Serial.println("✓ MQTT config saved to SPIFFS");
  return true;
}

// ===== SERVEUR HTTP (implémentation simple) =====

void handleWebServer(EthernetClient &client) {
  // Stub pour l'implémentation ultérieure du serveur web
}

#endif // WEB_CONFIG_H
