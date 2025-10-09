# ESP32-S3-ETH-8DI-8RO Controller

## 📋 Description

Contrôleur industriel pour carte Waveshare ESP32-S3-ETH-8DI-8RO avec Ethernet, MQTT, interface web et contrôle de 8 relais + 8 entrées digitales.

## ✨ Fonctionnalités

### ✅ Opérationnelles
- **🌐 Ethernet W5500** avec IP statique (192.168.1.50)
- **🔌 8 Relais contrôlables** via TCA9554 I2C (pins SDA=42, SCL=41)
- **📥 8 Entrées digitales** avec pull-up (pins 4-11)
- **📡 Interface MQTT** pour domotique/IoT
- **💻 Interface Web responsive** (http://192.168.1.50)
- **🔧 Interface série interactive** avec commandes complètes
- **⚡ API REST** pour intégration externe
- **🌡️ Capteur DHT22** température/humidité (pin 40)
- **🛠️ Système de diagnostic** avancé

## 🎮 Commandes Disponibles

```
help        - Affiche l'aide complète
status      - État du système complet
scan        - Scan des périphériques I2C
testio      - Test des entrées/sorties
pins        - Informations sur les pins
testpins    - Test différentes combinaisons I2C
relay X on  - Active le relais X (1-8)
relay X off - Désactive le relais X (1-8)
```

## 🌐 Interfaces Disponibles

### Interface Web
- **URL** : http://192.168.1.50
- **Contrôle visuel** des 8 relais
- **Monitoring** des 8 entrées digitales
- **Affichage** température/humidité
- **Actualisation automatique** toutes les 10s

### MQTT
- **Broker par défaut** : 192.168.1.100:1883
- **Topics** :
  - `esp32s3/relay/cmd` - Commandes relais
  - `esp32s3/relay/state` - États relais
  - `esp32s3/input/state` - États entrées
  - `esp32s3/sensor` - Données capteurs
  - `esp32s3/status` - État système

### API REST
- **Basculer relais** : `http://192.168.1.50/relay?num=1&action=toggle`
- **Basculer tous** : `http://192.168.1.50/relay?action=all_toggle`

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

### DHT22
- Data: Pin 12

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
5. **Modbus TCP** : `modbus` pour la configuration

### Contrôle Modbus TCP
```python
# Python avec pymodbus
from pymodbus.client.sync import ModbusTcpClient
client = ModbusTcpClient('192.168.1.50', port=502)

# Activer relais 1
client.write_coil(0, True)

# Lire entrées
inputs = client.read_discrete_inputs(10000, 8)
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

## 📊 État du Projet

| Composant | État | Notes |
|-----------|------|-------|
| TCA9554 Relais | ✅ OK | Pins officiels Waveshare |
| Entrées Digitales | ✅ OK | Pins 4-11 avec pull-up |
| Interface Série | ✅ OK | Commandes complètes |
| Diagnostic I2C | ✅ OK | Scan et test pins |
| Modbus TCP | ✅ OK | Port 502, registres configurés |
| Ethernet W5500 | 🔧 Config | Nécessite connexion physique |
| DHT22 | 🔧 Config | Pin 12 configuré |

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
