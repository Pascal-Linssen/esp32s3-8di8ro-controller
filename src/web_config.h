#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// Variables globales (déclarées dans main.cpp)
extern IPAddress mqttServer;
extern uint16_t mqttPort;
extern char mqttUser[50];
extern char mqttPassword[50];
extern char topicRelayCmd[100];
extern char topicRelayStatus[100];
extern char topicInputStatus[100];
extern char topicSensorStatus[100];
extern char topicSystemStatus[100];
extern PubSubClient mqttClient;
extern const char* CONFIG_FILE;

// ===== GESTION SPIFFS =====

void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("✗ SPIFFS init failed");
    return;
  }
  Serial.println("✓ SPIFFS ready");
}

void loadMQTTConfig() {
  if (!SPIFFS.exists(CONFIG_FILE)) {
    Serial.println("⚠️  Config file not found, using defaults");
    return;
  }
  
  File file = SPIFFS.open(CONFIG_FILE, "r");
  DynamicJsonDocument doc(512);
  
  if (deserializeJson(doc, file) == DeserializationError::Ok) {
    String ip = doc["broker_ip"] | "192.168.1.200";
    uint32_t ip_parts[4];
    sscanf(ip.c_str(), "%lu.%lu.%lu.%lu", &ip_parts[0], &ip_parts[1], &ip_parts[2], &ip_parts[3]);
    mqttServer = IPAddress(ip_parts[0], ip_parts[1], ip_parts[2], ip_parts[3]);
    
    mqttPort = doc["broker_port"] | 1883;
    strlcpy(mqttUser, doc["username"] | "pascal", sizeof(mqttUser));
    strlcpy(mqttPassword, doc["password"] | "123456", sizeof(mqttPassword));
    
    strlcpy(topicRelayCmd, doc["topic_relay_cmd"] | "home/esp32/relay/cmd", sizeof(topicRelayCmd));
    strlcpy(topicRelayStatus, doc["topic_relay_status"] | "home/esp32/relay/status", sizeof(topicRelayStatus));
    strlcpy(topicInputStatus, doc["topic_input_status"] | "home/esp32/input/status", sizeof(topicInputStatus));
    strlcpy(topicSensorStatus, doc["topic_sensor_status"] | "home/esp32/sensor/status", sizeof(topicSensorStatus));
    strlcpy(topicSystemStatus, doc["topic_system_status"] | "home/esp32/system/status", sizeof(topicSystemStatus));
    
    Serial.println("✓ MQTT config loaded from SPIFFS");
  }
  
  file.close();
}

void saveMQTTConfig() {
  DynamicJsonDocument doc(512);
  
  char ip_str[20];
  snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", 
    mqttServer[0], mqttServer[1], mqttServer[2], mqttServer[3]);
  
  doc["broker_ip"] = ip_str;
  doc["broker_port"] = mqttPort;
  doc["username"] = mqttUser;
  doc["password"] = mqttPassword;
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
