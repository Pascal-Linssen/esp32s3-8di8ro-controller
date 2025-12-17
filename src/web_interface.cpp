// ===== WEB INTERFACE - REST API HANDLERS =====
// HTTP server REST endpoints for relay control
// Note: getHtmlPage() is defined in main.cpp to avoid duplication

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Ethernet.h>

// Forward declarations
extern bool relayStates[8];
extern bool inputStates[8];
extern float temperature;
extern float humidity;
extern bool mqtt_connected;
extern bool eth_connected;
extern uint32_t loop_counter;
extern uint32_t callback_counter;
extern void setRelay(int relay, bool state);
extern String getHtmlPage();  // Defined in main.cpp

// Handler functions for REST API endpoints
void handleStatusRequest(EthernetClient &client) {
  // Generate JSON response
  DynamicJsonDocument doc(512);
  JsonArray r = doc.createNestedArray("r");
  JsonArray i = doc.createNestedArray("i");
  
  for (int j = 0; j < 8; j++) {
    r.add(relayStates[j] ? 1 : 0);
    i.add(inputStates[j] ? 1 : 0);
  }
  
  doc["t"] = temperature;
  doc["h"] = humidity;
  doc["u"] = millis();
  doc["l"] = loop_counter;
  doc["c"] = callback_counter;
  
  String json;
  serializeJson(doc, json);
  
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(json.length());
  client.println();
  client.println(json);
  client.flush();
}

void handleRelayRequest(EthernetClient &client, String &request) {
  // Parse request: /api/r/X/0|1|t  or /api/r/all/0|1|t
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Content-Length: 15");
  client.println();
  client.println("{\"status\":\"ok\"}");
  client.flush();
}

void handlePageRequest(EthernetClient &client) {
  String html = getHtmlPage();
  client.println(html);
  client.flush();
}
