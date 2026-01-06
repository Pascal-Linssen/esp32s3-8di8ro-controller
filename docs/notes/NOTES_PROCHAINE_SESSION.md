# 📝 NOTES POUR PROCHAINE SESSION

**Date**: 16 Décembre 2025 (Après-midi)  
**Statut**: Code committé et pushé sur GitHub  
**Prochaine session**: À déterminer

---

## 🎯 Problème Principal À Résoudre

### MQTT Commands Callback Not Triggered

**Symptôme**: Toutes les commandes MQTT sont publiées au broker mais **jamais reçues** par l'ESP32

**Preuve du problème**:
```
✅ Statuts publient correctement (MQTT Explorer les reçoit)
✅ Ethernet fonctionne (IP: 192.168.1.50)
✅ Credentials correctes (<mqtt_username> / <mqtt_password>)
✅ Client MQTT reste connecté (mqttClient.connected() = true)
❌ Callback JAMAIS appelée (aucun debug "📨 MQTT Reçu" dans logs)
❌ Relays ne changent pas d'état
```

**Test confirmé**: 
- Relay 0 s'est allumé UNE FOIS avec `0:on` (preuve que le système peut fonctionner)
- Puis plus rien avec les autres commandes/relays
- Monitoring serial ne montre JAMAIS les logs de callback

---

## ⚡ Actions Rapides À Faire (15-20 min)

### 1. **Ajouter debug massif**
```cpp
// Dans loop(), AVANT Ethernet.maintain():
static uint32_t loop_counter = 0, callback_counter = 0;
if (loop_counter++ % 500 == 0) {
    Serial.printf("[LOOP %lu] MQTT=%d Subscribe=%u Callbacks=%u\n", 
        loop_counter, mqttClient.connected(), sub_count, callback_counter);
}

// Dans mqttCallback():
callback_counter++;
```

### 2. **Vérifier subscribe() return value**
```cpp
int sub_result = mqttClient.subscribe(topicRelayCmd);
Serial.printf("Subscribe result: %d (expect > 0)\n", sub_result);
```

### 3. **Forcer resubscription en boucle**
```cpp
static uint32_t last_subscribe = 0;
if (millis() - last_subscribe > 30000) {  // Tous les 30s
    last_subscribe = millis();
    Serial.println("Resubscribe forcée...");
    mqttClient.unsubscribe(topicRelayCmd);
    delay(100);
    mqttClient.subscribe(topicRelayCmd);
}
```

### 4. **Tester avec mosquitto sur PC**
```bash
# Terminal 1: Écouter les commandes reçues par broker
mosquitto_sub -h 192.168.1.200 -u <mqtt_username> -P <mqtt_password> -t "waveshare/relay/cmd" -v

# Terminal 2: Envoyer une commande
mosquitto_pub -h 192.168.1.200 -u <mqtt_username> -P <mqtt_password> -t "waveshare/relay/cmd" -m "0:on"

# Remplace par tes valeurs (exemple):
# mosquitto_sub -h 192.168.1.200 -u <mqtt_username> -P <mqtt_password> -t "waveshare/relay/cmd" -v
# mosquitto_pub -h 192.168.1.200 -u <mqtt_username> -P <mqtt_password> -t "waveshare/relay/cmd" -m "0:on"
```
→ Si mosquitto_sub reçoit le message mais ESP32 non = problème subscribe  
→ Si mosquitto_sub ne reçoit rien = problème publish côté client test

---

## 🔍 Hypothèses À Investiguer

### Hypothèse 1: Topic mismatch
- ESP32 subscribe à: `waveshare/relay/cmd` 
- On publie à: `waveshare/relay/cmd`
- **Verdict**: À confirmer avec debug

### Hypothèse 2: PubSubClient.loop() ne déclenche pas callback
- Peut être un problème de version (2.8.0 actuelle)
- **Test**: Ajouter debug dans mqttClient.loop()

### Hypothèse 3: Subscription non active
- subscribe() retourne peut-être 0 ou -1
- **Test**: Print le return value

### Hypothèse 4: Callback pas assignée correctement
```cpp
// Dans setupMqtt():
mqttClient.setCallback(mqttCallback);  // C'EST CRUCIAL!
```
- À vérifier si cette ligne est bien présente

---

## 📂 Fichiers Clés

- `src/main.cpp` - Cœur du firmware (569 lignes)
- `src/web_config.h` - SPIFFS config management
- `docs/notes/SESSION_SUMMARY.md` - Résumé complet de la session

## 🔗 GitHub
- Repo: https://github.com/Pascal-Linssen/esp32s3-8di8ro-controller
- Branch: main
- Latest: `0f3a6c6` (docs update)

---

## ✅ Qu'est-ce qui fonctionne (À NE PAS TOUCHER)

```cpp
✅ Hardware: relays, inputs, sensors
✅ Ethernet: stable, IP 192.168.1.50
✅ MQTT Publish: status reçus parfaitement
✅ MQTT Connect: authentification OK
✅ SPIFFS: load/save config fonctionne
✅ Serial CLI: contrôle local OK
```

---

## ❌ Qu'est-ce qui ne fonctionne pas (À FIXER)

```cpp
❌ MQTT Subscribe callback
❌ MQTT Commands reception  
❌ Relay state changes via MQTT
❌ Web interface (stub only)
```

---

## 🚀 Si callback fix fonctionne (happy path)

1. Tester ALL:on, ALL:off
2. Tester chaque relay individuellement
3. Valider persistence (commands avant reboot, check après reboot)
4. Puis: Web interface + dashboard

---

## 📞 Support Notes

- **Broker**: 192.168.1.200:1883 (Mosquitto local)
- **Credentials**: <mqtt_username> / <mqtt_password>
- **ESP32 Serial**: COM4 @ 9600 baud
- **Monitor**: `platformio device monitor -p COM4 -b 9600`
- **Upload**: `platformio run -e esp32s3 -t upload`

---

*Generated: 16-Dec-2025*
