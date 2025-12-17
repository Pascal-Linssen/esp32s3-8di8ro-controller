# Guide de Migration PubSubClient → 256dpi/MQTT

## ⚡ Résumé Rapide

**Problème** : PubSubClient v2.8.0 les callbacks MQTT ne fonctionnent jamais sur ESP32 + Ethernet W5500

**Solution** : Utiliser `256dpi/arduino-mqtt` v2.5.2 qui supporte correctement Ethernet sur ESP32

**Temps de migration** : 5-10 minutes

---

## 📋 Checklist de Migration

- [ ] **Étape 1** : Sauvegarder les fichiers actuels
- [ ] **Étape 2** : Mettre à jour `platformio.ini`
- [ ] **Étape 3** : Adapter le code
- [ ] **Étape 4** : Compiler et tester

---

## 🔧 Étape 1 : Sauvegarder

```bash
# Dans le dossier du projet
cp src/main.cpp src/main_OLD_pubsub.cpp
cp platformio.ini platformio.ini.backup
```

---

## ⚙️ Étape 2 : platformio.ini

**AVANT** :
```ini
lib_deps =
  Wire
  SPI
  Ethernet @ ^2.0.2
  bblanchon/ArduinoJson @ ^6.21.4
  https://github.com/RobTillaart/TCA9554.git
  adafruit/DHT sensor library @ ^1.4.6
  emelianov/modbus-esp8266 @ ^4.1.0
  adafruit/Adafruit Unified Sensor @ ^1.1.14
  knolleary/PubSubClient @ ^2.8.0     ← SUPPRIMER
  SPIFFS
```

**APRÈS** :
```ini
lib_deps =
  Wire
  SPI
  Ethernet @ ^2.0.2
  bblanchon/ArduinoJson @ ^6.21.4
  https://github.com/RobTillaart/TCA9554.git
  adafruit/DHT sensor library @ ^1.4.6
  emelianov/modbus-esp8266 @ ^4.1.0
  adafruit/Adafruit Unified Sensor @ ^1.1.14
  256dpi/MQTT @ ^2.5.2                ← AJOUTER
  SPIFFS
```

---

## 💻 Étape 3 : Adapter le Code

### 3.1 - Includes

**AVANT** :
```cpp
#include <PubSubClient.h>
```

**APRÈS** :
```cpp
#include <MQTT.h>
```

---

### 3.2 - Déclaration Clients

**AVANT** :
```cpp
EthernetClient ethClient;
PubSubClient mqttClient(ethClient);
```

**APRÈS** :
```cpp
EthernetClient net;           // Nommer 'net' (convention 256dpi)
MQTTClient mqttClient(256);   // Taille buffer en paramètre
```

---

### 3.3 - Callback MQTT

**AVANT** :
```cpp
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr(topic);
  String payloadStr("");
  for (unsigned int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }
  
  // ... votre logique ...
}
```

**APRÈS** (beaucoup plus simple) :
```cpp
void messageReceived(String &topic, String &payload) {
  // 'topic' et 'payload' sont déjà des String !
  // ... votre logique (même chose) ...
}
```

**Changements** :
- Signature fonction différente
- `topic` et `payload` sont des `String` directement
- Plus de boucle pour construire le payload string
- Plus simple !

---

### 3.4 - Setup MQTT

**AVANT** :
```cpp
void setup() {
  // ... setup autre hardware ...
  
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
}
```

**APRÈS** :
```cpp
void setup() {
  // ... setup autre hardware ...
  
  mqttClient.begin(mqtt_server, mqtt_port, net);
  mqttClient.onMessage(messageReceived);
  
  Serial.println("✓ MQTT client initialized");
}
```

---

### 3.5 - Loop Principal

**AVANT** :
```cpp
void loop() {
  Ethernet.maintain();
  
  if (!mqttClient.connected()) {
    reconnectMqtt();
  }
  
  mqttClient.loop();
  delay(10);
  
  // ... reste du code ...
}
```

**APRÈS** (essentiellement identique) :
```cpp
void loop() {
  Ethernet.maintain();
  
  mqttClient.loop();  // Gère les callbacks ET maintient la connexion
  delay(10);
  
  if (!mqttClient.connected()) {
    connectMqtt();  // Optionnel, loop() gère aussi la reconnexion
  }
  
  // ... reste du code ...
}
```

---

### 3.6 - Reconnexion MQTT

**AVANT** :
```cpp
void reconnectMqtt() {
  if (mqttClient.connected() || !eth_connected) return;
  
  mqtt_reconnects++;
  
  Serial.printf("🔄 MQTT reconnect attempt #%lu\n", mqtt_reconnects);
  
  if (mqttClient.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
    Serial.println("✅ MQTT connected!");
    mqttClient.subscribe(topic_relay_cmd);
  } else {
    Serial.printf("❌ MQTT connect failed (state=%d)\n", mqttClient.state());
    delay(500);
  }
}
```

**APRÈS** (presque identique) :
```cpp
void connectMqtt() {
  if (mqttClient.connected() || !eth_connected) return;
  
  mqtt_reconnects++;
  
  Serial.printf("🔄 MQTT connect attempt #%lu\n", mqtt_reconnects);
  
  if (mqttClient.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
    Serial.println("✅ MQTT connected!");
    mqttClient.subscribe(topic_relay_cmd);
  } else {
    Serial.printf("❌ MQTT connect failed\n");
    // Pas de .state() pour 256dpi, mais pas grave
    delay(500);
  }
}
```

**Différences** :
- Pas de `.state()` disponible (pas grave, on voit l'erreur de toute façon)
- Sinon identique

---

### 3.7 - Publish MQTT

**AVANT** :
```cpp
mqttClient.publish(topic_relay_status, relayStr.c_str());
```

**APRÈS** :
```cpp
mqttClient.publish(topic_relay_status, relayStr);  // String directement
// OU
mqttClient.publish(topic_relay_status, relayStr.c_str());  // Les deux fonctionnent
```

---

## 🧪 Étape 4 : Compiler et Tester

### Compiler
```bash
pio run
```

Si erreurs :
```bash
# Nettoyer et recompiler
pio run --target clean
pio run
```

### Tester

1. **Upload** le code
2. **Ouvrir** la console série
3. **Attendre** que MQTT se connecte
4. **Envoyer** un message MQTT :
   ```bash
   mosquitto_pub -h 192.168.1.200 -u <mqtt_username> -P <mqtt_password> \
    -t waveshare/relay/cmd -m "0:on"
   ```
5. **Vérifier** la console : vous devriez voir
   ```
   🎯 MQTT MESSAGE RECEIVED #1!
      Topic: waveshare/relay/cmd
      Payload: 0:on
   ✓ Relay 0: ON
   ```

---

## 🎯 Différences Clés à Retenir

| Aspect | PubSubClient | 256dpi/MQTT |
|--------|---|---|
| Include | `<PubSubClient.h>` | `<MQTT.h>` |
| Client Network | `EthernetClient ethClient` | `EthernetClient net` |
| Client MQTT | `PubSubClient mqttClient(ethClient)` | `MQTTClient mqttClient(256)` |
| Init | `setServer()` + `setCallback()` | `begin()` + `onMessage()` |
| Callback Signature | `void cb(char*, byte*, uint)` | `void cb(String&, String&)` |
| Setup Callback | `setCallback(mqttCallback)` | `onMessage(messageReceived)` |
| Reconnect Auto | Non | Oui (dans loop()) |
| Bug Ethernet | ❌ OUI (#1087, #1070) | ✅ NON |

---

## 🔍 Troubleshooting Compilation

### Erreur 1 : "lwmqtt.h: No such file"
```
→ Faire : pio run --target clean && pio run
```

### Erreur 2 : "undefined reference to MQTTClient"
```
→ Vérifier que lib_deps a "256dpi/MQTT @ ^2.5.2"
→ Faire : pio lib update
```

### Erreur 3 : "messageReceived not declared"
```
→ Ajouter avant setup() : void messageReceived(String &topic, String &payload);
```

---

## ✅ Validation

Après migration, vous devriez voir :

✅ Compilation réussit
✅ Ethernet se connecte
✅ MQTT se connecte et subscribe réussit
✅ **LES CALLBACKS SE DÉCLENCHENT ENFIN** 🎉
✅ Les relais répondent aux commandes MQTT

---

## 📝 Fichiers de Référence

- **Code complet** : `src/main_mqtt_fixed.cpp`
- **Config complet** : `platformio_mqtt_fixed.ini`
- **Documentation** : `docs/MQTT_SOLUTION_ANALYSIS.md`

---

## 💡 FAQ

**Q : Est-ce que 256dpi/MQTT est stable ?**
A : Oui, 1.1k stars, activement maintenue, beaucoup d'exemples

**Q : Est-ce que mon code va fonctionner sans changements?**
A : Non, mais les changements sont très mineurs (voir Étape 3)

**Q : Est-ce que j'ai besoin de changer le hardware ?**
A : Non, aucun changement hardware, juste software

**Q : Est-ce que les topics MQTT sont identiques ?**
A : Oui, identiques, aucun changement

**Q : Comment je fais pour revenir à PubSubClient si ça marche pas ?**
A : Vous avez une backup : `src/main_OLD_pubsub.cpp`

---

## 🎓 Apprentissage

Si vous voulez comprendre pourquoi PubSubClient avait ce bug :
- Lire Issue #1070 : Conflit SPI transaction avec Ethernet
- Lire Issue #1087 : Callbacks ne se déclenchent pas
- 256dpi/MQTT gère mieux la séparation entre couches TCP et MQTT

Bonne migration ! 🚀
