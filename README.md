# ESP32-S3-ETH-8DI-8RO Controller

# ESP32-S3-ETH-8DI-8RO Controller

## 📋 Description

✅ **SYSTÈME OPÉRATIONNEL** - Contrôleur industriel pour carte Waveshare ESP32-S3-ETH-8DI-8RO avec Ethernet, MQTT et contrôle de 8 relais + 8 entrées digitales.

## ✨ Fonctionnalités

### ✅ Opérationnelles (v1.5 - SESSION 3)
- **🌐 Ethernet W5500** avec IP statique (192.168.1.50) - TESTÉ ✓
- **🔌 8 Relais contrôlables** via TCA9554 I2C (pins SDA=42, SCL=41) - **TOUS LES 8 TESTÉS ✓**
- **📥 8 Entrées digitales** avec pull-up interne (pins 4-11) - TOUS LES 8 TESTÉS ✓
- **🌡️ Capteur DHT22** température/humidité (pin 40)
- **🔧 Interface série CLI** avec commandes de contrôle
- **📊 Boucle stable** avec polling 2s (sensors/inputs)
- **💾 Très bon rendement mémoire** : 5.9% RAM, 8.8% Flash utilisés

### 🔄 En Développement (SESSION 3+)
- **💻 Interface Web HTTP** - Architecture prête, besoin bibliothèque AsyncWebServer
- **⚡ API REST** pour intégration externe
- **📡 MQTT Integration** - Broker credentials ready (192.168.1.200:1883, pascal/123456)
- **🏭 Modbus TCP** (future)

## 🎮 Commandes Disponibles (Série - 9600 baud)

```
help              - Affiche l'aide
relay X on        - Allume relais X (0-7)
relay X off       - Éteint relais X (0-7)
test              - Cycle tous les relais pour test
```

## 🌐 Interfaces Disponibles

### Interface Web
- **URL** : http://192.168.1.50
- **Contrôle visuel** des 8 relais
- **Monitoring** des 8 entrées digitales
- **Affichage** température/humidité
- **Actualisation automatique** toutes les 10s

### MQTT
- **Broker** : 192.168.1.200:1883 (pascal/123456)
- **Topics** :
  - `esp32s3/relay/cmd` - Commandes relais JSON/simple
  - `esp32s3/relay/status` - États relais
  - `esp32s3/sensor` - Température, humidité, entrées
- **Formats supportés** :
  - JSON : `{"relay": 1, "state": "on"}`
  - Simple : `1:ON`
  - `esp32s3/sensor` - Données capteurs
  - `esp32s3/status` - État système

## 📌 Configuration Pins

### Ethernet W5500
- CS: Pin 16
- RST: Pin 39  
- SCK: Pin 15
- MISO: Pin 14
- MOSI: Pin 13

### TCA9554 I2C (Relais)
- SDA: Pin 42 ⚡
- SCL: Pin 41 ⚡

### Entrées Digitales
- IN1-8: Pins 4-11

### DHT22
- Data: Pin 40

## 🚀 Installation

### Prérequis
```bash
# Bibliothèques PlatformIO
- Wire @ 2.0.0
- Ethernet @ 2.0.2
- TCA9554 @ 0.1.2+sha.79c8c0b
- DHT sensor library @ 1.4.6
- Adafruit Unified Sensor @ 1.1.15
- modbus-esp32 @ 4.1.0
```

### Configuration PlatformIO
```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 9600
build_flags = 
    -DARDUINO_USB_CDC_ON_BOOT=1
```

### Compilation et Upload
```bash
platformio run --target upload
platformio device monitor --port COM8 --baud 9600
```

## 💡 Utilisation

1. **Connexion série** : 9600 bauds
2. **Test des relais** : `relay 1 on`, `relay 1 off`
3. **État système** : `status`
4. **Diagnostic I2C** : `scan`
5. **Contrôle MQTT** : Topics `esp32s3/relay/cmd`

### Contrôle MQTT
```bash
# Avec Mosquitto clients
mosquitto_pub -h 192.168.1.200 -u pascal -P 123456 -t "esp32s3/relay/cmd" -m "1:ON"
mosquitto_pub -h 192.168.1.200 -u pascal -P 123456 -t "esp32s3/relay/cmd" -m "ALL:OFF"
```

## 🔍 Diagnostic

### Vérifier l'I2C
```
scan
```
Résultat attendu : TCA9554 détecté à l'adresse 0x20

### Test des entrées/sorties
```
testio
```

### Informations sur les pins
```
pins
```

## 🎯 Résolution de Problèmes

### TCA9554 non détecté
- Vérifier les pins I2C : SDA=42, SCL=41
- Scanner avec `testpins` pour tester d'autres combinaisons

### Ethernet non fonctionnel
- Vérifier la connexion du câble Ethernet
- Pins W5500 configurés selon schéma Waveshare officiel

## 📊 État du Projet (SESSION 3 UPDATES)

| Composant | État | Notes |
|-----------|------|-------|
| TCA9554 Relais (8x) | ✅ OPÉRATIONNEL | Tous testés via série - I2C @ 0x20 |
| Entrées Digitales (8x) | ✅ OPÉRATIONNEL | Toutes 8 lisent correctement |
| Ethernet W5500 | ✅ CONNECTÉ | IP 192.168.1.50, stable |
| Interface Série CLI | ✅ OPÉRATIONNEL | Commandes relay/test/help |
| Capteur DHT22 | 🟡 CONFIG | Initialized, mais sensor non physiquement détecté |
| Interface Web HTTP | 🟡 EN COURS | HTML/CSS prêts, besoin serveur HTTP |
| API REST | ⏳ À FAIRE | Design prêt, implémentation après HTTP |
| MQTT Integration | ⏳ À FAIRE | Broker credentials: 192.168.1.200:1883 |
| Modbus TCP | ⏳ À FAIRE | Future enhancement |

## 🧪 Résultats de Test (SESSION 3)

**Test de Relais via CLI Sérielle:**
```
>>> relay 0 on
✓ Relais 1: ON (TCA9554 @ 0x20 bit 0)
>>> relay 0 off
✓ Relais 1: OFF (TCA9554 @ 0x20 bit 0)
>>> test
✓ All 8 relays cycled ON/OFF successfully
```

**Lecture des Entrées:**
```
Entrées: 1 1 1 1 1 0 1 1  ← Entrée 6 détectée LOW (physique confirmée)
```

**Métriques de Performance:**
- RAM: 19.3KB / 320KB (5.9%)
- Flash: 302KB / 3.3MB (8.8%)
- Boot time: ~2s
- Loop rate: 2s (polling)
- Compilation: 9-20s

## 🏗️ Architecture

```
ESP32-S3-ETH-8DI-8RO
├── TCA9554 (I2C 0x20) → 8 Relais
├── Entrées digitales → Pins 4-11  
├── W5500 (SPI) → Ethernet
├── Modbus TCP → Port 502 (Coils 0-7, Inputs 10000-10007)
├── DHT22 → Pin 12
└── Interface série → Diagnostic
```

## 🤝 Contribution

Développé avec la découverte des pins officiels Waveshare via leur démo Arduino.

## 📜 Licence

Projet libre d'utilisation pour applications industrielles et éducatives.

---

**Version**: 1.0 - TCA9554 Fonctionnel  
**Carte**: Waveshare ESP32-S3-ETH-8DI-8RO  
**Framework**: Arduino/PlatformIO
