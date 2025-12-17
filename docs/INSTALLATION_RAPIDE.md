# ⚡ Installation Rapide - 5 Minutes

## Résumé
Remplacer PubSubClient (qui a des bugs) par 256dpi/MQTT (qui fonctionne avec Ethernet)

---

## 🚀 Étapes Rapides

### 1️⃣ Sauvegarder
```bash
cp src/main.cpp src/main.cpp.OLD
```

### 2️⃣ Remplacer le Code

**Copier `main_mqtt_fixed.cpp` vers `src/main.cpp`** :
```bash
cp src/main_mqtt_fixed.cpp src/main.cpp
```

### 3️⃣ Mettre à Jour platformio.ini

**Ouvrir `platformio.ini` et remplacer la section `lib_deps` par** :

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
  256dpi/MQTT @ ^2.5.2
  SPIFFS
```

**Ou simplement : Changer cette ligne** :
```diff
- knolleary/PubSubClient @ ^2.8.0
+ 256dpi/MQTT @ ^2.5.2
```

### 4️⃣ Compiler
```bash
pio run
```

### 5️⃣ Upload
```bash
pio run --target upload
```

### 6️⃣ Tester
```bash
python3 test_mqtt_fixed.py
```

---

## ✅ Vérification

Ouvrez la console et vous devriez voir :
```
🎯 MQTT MESSAGE RECEIVED #1!
   Topic: home/esp32/relay/cmd
   Payload: 0:on
✓ Relay 0: ON
```

**Si vous voyez ça → LES CALLBACKS FONCTIONNENT !** 🎉

---

## 🆘 Troubleshooting

### ❌ Erreur : "lwmqtt.h: No such file"
```bash
pio run --target clean
pio run
```

### ❌ Erreur : "undefined reference"
- Vérifier que `platformio.ini` a bien `256dpi/MQTT @ ^2.5.2`
- Faire : `pio lib update`

### ❌ MQTT ne se connecte pas
- Vérifier IP du broker dans le code
- Vérifier user/password
- Vérifier que Ethernet fonctionne d'abord

---

## 📁 Fichiers Fournis

| Fichier | Description |
|---------|---|
| `src/main_mqtt_fixed.cpp` | ✅ **Nouveau code complet (UTILISER CELUI-CI)** |
| `platformio_mqtt_fixed.ini` | Référence config (copier dans platformio.ini) |
| `docs/MIGRATION_GUIDE.md` | Guide détaillé des changements |
| `docs/MQTT_SOLUTION_ANALYSIS.md` | Analyse du problème |
| `docs/EXTERNAL_RESEARCH.md` | Ressources trouvées |
| `test_mqtt_fixed.py` | Script de test |

---

## 🎯 Différence Clé

### AVANT (PubSubClient - BUG)
```cpp
#include <PubSubClient.h>
EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Callbacks JAMAIS appelés ❌
}
```

### APRÈS (256dpi/MQTT - FONCTIONNE)
```cpp
#include <MQTT.h>
EthernetClient net;
MQTTClient mqttClient(256);

void messageReceived(String &topic, String &payload) {
  // Callbacks TOUJOURS appelés ✅
}
```

---

## 📞 Questions ?

Consulter :
- `docs/MIGRATION_GUIDE.md` pour les détails
- `docs/MQTT_SOLUTION_ANALYSIS.md` pour comprendre pourquoi
- `src/main_mqtt_fixed.cpp` pour voir le code complet

---

C'est tout ! Vous avez 5 minutes pour faire ça. 🚀
