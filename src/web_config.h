#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

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
extern const char* CONFIG_FILE;

// ===== GESTION SPIFFS =====

void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("✗ SPIFFS init failed");
    return;
  }
  Serial.println("✓ SPIFFS ready");
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
  if (!SPIFFS.exists(CONFIG_FILE)) {
    Serial.println("⚠️  Config file not found, using defaults");
    return;
  }
  
  File file = SPIFFS.open(CONFIG_FILE, "r");
  DynamicJsonDocument doc(512);
  
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
    strlcpy(mqttUser, doc["username"] | "", sizeof(mqttUser));
    strlcpy(mqttPassword, doc["password"] | "", sizeof(mqttPassword));
    strlcpy(otaKey, doc["ota_key"] | "", sizeof(otaKey));
    
    strlcpy(topicRelayCmd, doc["topic_relay_cmd"] | "waveshare/relay/cmd", sizeof(topicRelayCmd));
    strlcpy(topicRelayStatus, doc["topic_relay_status"] | "waveshare/relay/status", sizeof(topicRelayStatus));
    strlcpy(topicInputStatus, doc["topic_input_status"] | "waveshare/input/status", sizeof(topicInputStatus));
    strlcpy(topicSensorStatus, doc["topic_sensor_status"] | "waveshare/sensor/status", sizeof(topicSensorStatus));
    strlcpy(topicSystemStatus, doc["topic_system_status"] | "waveshare/system/status", sizeof(topicSystemStatus));

    // Migration automatique: ancien préfixe -> nouveau
    migrateTopicPrefix(topicRelayCmd, sizeof(topicRelayCmd), "home/esp32/", "waveshare/");
    migrateTopicPrefix(topicRelayStatus, sizeof(topicRelayStatus), "home/esp32/", "waveshare/");
    migrateTopicPrefix(topicInputStatus, sizeof(topicInputStatus), "home/esp32/", "waveshare/");
    migrateTopicPrefix(topicSensorStatus, sizeof(topicSensorStatus), "home/esp32/", "waveshare/");
    migrateTopicPrefix(topicSystemStatus, sizeof(topicSystemStatus), "home/esp32/", "waveshare/");
    
    Serial.println("✓ MQTT config loaded from SPIFFS");
  }
  
  file.close();
}

void saveMQTTConfig() {
  DynamicJsonDocument doc(512);
  
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
  
  File file = SPIFFS.open(CONFIG_FILE, "w");
  serializeJson(doc, file);
  file.close();
  
  Serial.println("✓ MQTT config saved to SPIFFS");
}

// ===== SERVEUR HTTP (implémentation simple) =====

void handleWebServer(EthernetClient &client) {
  // Stub pour l'implémentation ultérieure du serveur web
}

#endif // WEB_CONFIG_H
