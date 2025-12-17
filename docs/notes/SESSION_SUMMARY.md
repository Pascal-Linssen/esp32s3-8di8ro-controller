# Résumé Session 16 Décembre 2025

## ✅ COMPLÉTÉ

### Hardware (Validé)
- ✅ ESP32-S3 @ 192.168.1.50 (Ethernet W5500)
- ✅ 8 Relays TCA9554 @ I2C 0x20 (SDA=42, SCL=41)
- ✅ 8 Inputs digitales (Pull-Up interne)
- ✅ DHT22 Temperature/Humidité
- ✅ Tous les contrôles individuels fonctionnent

### Logiciel v1.6
- ✅ Configuration MQTT persistente via SPIFFS (/config.json)
- ✅ Chargement des paramètres au démarrage
- ✅ Publication des status (5 topics MQTT)
- ✅ Support des variables mutables (pas de recompilation requise pour config)
- ✅ Python CLI tool (configure_mqtt.py) pour editer config
- ✅ Compilation optimisée: 385KB flash, 6.1% RAM

### Credentials (CORRIGÉS)
```
Broker: 192.168.1.200:1883
User: pascal
Password: 123456 (NON: Europe3224.@)
Client ID: ESP32-S3-ETH
```

### MQTT Topics (Fonctionnels)
- ✅ `home/esp32/relay/status` - Publication JSON array [0,0,0,0,0,0,0,0]
- ✅ `home/esp32/input/status` - Entrées digitales
- ✅ `home/esp32/sensor/status` - Température/Humidité
- ✅ `home/esp32/system/status` - Infos système
- 🟡 `home/esp32/relay/cmd` - **PROBLÉMATIQUE** (voir section suivante)

---

## ⚠️ EN INVESTIGATION - MQTT COMMANDS

### Symptôme
Les commandes MQTT ne sont **pas reçues** par la callback:
- Format: `0:on`, `1:off`, `ALL:on` etc.
- Publiées au bon topic
- ESP32 reste connecté (mqttClient.connected() = true)
- **Status publie correctement** (Ethernet/MQTT fonctionne)
- **Callback jamais appelée** (aucun debug serial)

### Investigation Faite
1. ✅ Credentials corrigées (123456)
2. ✅ SPIFFS effacé et réinitialisé
3. ✅ Firmware compilé avec debug extensive
4. ✅ Monitoring serial actif
5. ✅ Test: Relay 0 s'est allumé UNE FOIS puis plus rien
6. ✅ Vérification: 3 relays testés (2,4,5,6) - aucun changement

### Cause Probable
- Callback `mqttCallback()` n'est pas appelée bien que la subscription soit active
- Soit: Topic mismatch, Soit: Subscribe non active, Soit: PubSubClient.loop() ne trigger pas callback

### À Tester (Prochainement)
1. Vérifier si mqttClient.subscribe() retourne un value > 0
2. Ajouter debug avant/après mqttClient.loop()
3. Forcer une unsubscribe/subscribe
4. Tester avec mosquitto_sub sur PC pour confirmer broker reçoit les messages
5. Vérifier version PubSubClient (actuellement 2.8.0)

---

## 📋 À FAIRE

### URGENT (Pour lundi)
1. **Déboguer callback MQTT**
   - Ajouter Serial debug: "Loop: X" chaque 100ms
   - Ajouter counter de callback triggers
   - Recréer subscription en loop() si count = 0

2. **Valider commandes**
   - Une fois callback fixed: tester ALL:on, ALL:off
   - Tester chaque relay individuellement
   - Valider persistence des états

3. **WebSocket/HTTP Config** (OPTIONNEL)
   - Stubé actuellement dans handleWebServer()
   - Permettrait edit config via navigateur sans Python CLI

### NORMAL (À faire)
1. Améliorer debug logging
2. Ajouter watchdog timer
3. Ajouter Web interface pour dashboard
4. Home Assistant MQTT discovery

### LOW PRIORITY
1. Chiffrer credentials en SPIFFS
2. OTA firmware updates
3. Backup/restore config

---

## 📂 Structure Fichiers

```
src/
├── main.cpp (569 lignes)
│   ├── Hardware init (I2C, SPI, Ethernet, DHT)
│   ├── MQTT (connect, publish, subscribe)
│   ├── Web config stub
│   └── Serial commands
├── web_config.h (85 lignes)
│   ├── initSPIFFS()
│   ├── loadMQTTConfig()
│   ├── saveMQTTConfig()
│   └── handleWebServer() [STUB]

Scripts/
├── configure_mqtt.py - CLI pour editer config
├── mqtt_test.py - Test connectivity
├── erase_spiffs.py - Wipe SPIFFS
└── test_cli_serial.py - Serial commands

Docs/
├── CONFIG_MQTT.md - Documentation config
├── SESSION_SUMMARY.md (this file)

Config/
└── platformio.ini - Optimisé pour ESP32-S3
```

---

## 🔧 Prochaines Étapes (Recommandées)

### Session Prochaine
1. **Ajouter debug massif** dans loop():
   ```cpp
   static uint32_t dbg_count = 0;
   if (dbg_count++ % 100 == 0) {
       Serial.printf("Loop %lu: connected=%d callback_count=%u\n", 
           dbg_count, mqttClient.connected(), callback_triggers);
   }
   ```

2. **Forcer resubscription** si callback = 0:
   ```cpp
   if (callback_count == 0 && millis() > 15000) {
       Serial.println("!!! Resubscribe forcée");
       mqttClient.unsubscribe(topicRelayCmd);
       delay(100);
       mqttClient.subscribe(topicRelayCmd);
   }
   ```

3. **Tester avec mosquitto** sur PC:
   ```bash
   mosquitto_sub -h 192.168.1.200 -u pascal -P 123456 -t "home/esp32/relay/cmd" -v
   # Puis envoyer: mosquitto_pub ... -m "0:on"
   ```

4. **Si callback ne s'appelle toujours pas**:
   - Vérifier library PubSubClient version
   - Essayer avec AsyncMqttClient à la place
   - Vérifier connection state APRÈS subscribe

---

## 📊 Status Récapitulatif

| Composant | État | Notes |
|-----------|------|-------|
| Hardware | ✅ | Tous les relays testés individuellement |
| Ethernet | ✅ | Ping, DNS, DHCP OK |
| MQTT Connect | ✅ | Auth OK, client reste connecté |
| MQTT Publish | ✅ | Status reçus dans MQTT Explorer |
| MQTT Subscribe | ❓ | Active mais callback jamais appelée |
| MQTT Commands | ❌ | Relays ne changent pas d'état |
| I2C/TCA9554 | ✅ | Direct wire.write() fonctionne |
| SPIFFS Config | ✅ | Load/Save OK |
| Serial CLI | ✅ | Commandes locales fonctionnent |
| Web Interface | ❌ | Stub seulement |

---

## 🚀 Deployment

- **Broker**: 192.168.1.200:1883 (Mosquitto local)
- **ESP32 IP**: 192.168.1.50
- **Upload Port**: COM4 (115200 baud)
- **Monitor**: 9600 baud
- **Firmware Size**: 385KB / 3.3MB available

---

**Dernière mise à jour**: 16 Dec 2025, Session après-midi
**Prochaine action**: Déboguer callback MQTT
