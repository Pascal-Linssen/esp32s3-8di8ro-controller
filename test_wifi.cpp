/*
 * Test simple WiFi pour ESP32-S3-ETH-8DI-8RO
 * Ce fichier peut être utilisé pour tester indépendamment le WiFi
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

const char* AP_SSID = "ESP32-ETH-8DI8RO";
const char* AP_PASS = "changeme123";

WebServer server(80);

void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html>
<head><title>ESP32-S3-ETH-8DI-8RO Test</title></head>
<body>
<h1>Test WiFi OK!</h1>
<p>Le serveur Web fonctionne correctement.</p>
</body>
</html>
)";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n=== Test WiFi ESP32-S3-ETH-8DI-8RO ===");
  
  // Mode AP
  WiFi.mode(WIFI_AP);
  if (WiFi.softAP(AP_SSID, AP_PASS)) {
    Serial.println("✓ Point d'accès créé");
    Serial.print("SSID: "); Serial.println(AP_SSID);
    Serial.print("Pass: "); Serial.println(AP_PASS);
    Serial.print("IP: "); Serial.println(WiFi.softAPIP());
    
    // Routes
    server.on("/", handleRoot);
    server.begin();
    
    Serial.println("✓ Serveur démarré");
    Serial.println("=== Test terminé ===");
  } else {
    Serial.println("✗ Échec création AP");
  }
}

void loop() {
  server.handleClient();
  
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 10000) {
    lastPrint = millis();
    Serial.println("[WiFi Test] Serveur actif...");
  }
}