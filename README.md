# ESP32-S3-ETH-8DI-8RO Controller

Contrôleur industriel pour la carte Waveshare **ESP32‑S3‑ETH‑8DI‑8RO**.

- Ethernet **W5500** (IP statique configurable)
- **8 relais** via **TCA9554 (I2C 0x20)**
- **8 entrées digitales** (GPIO 4 à 11)
- **MQTT** (publication + commandes relais)
- Interface **Web** (dashboard + configuration) via Ethernet
- Configuration persistée dans **SPIFFS** (`/config.json`)
- Capteur **DHT22** optionnel (DATA sur **GPIO21**)

## Statut

- Matériel (Ethernet, relais, entrées) : OK
- MQTT : OK
- Web (dashboard + API `GET /api/status`) : OK
- Configuration Web (réseau + MQTT) : OK

## Build / Flash (PlatformIO)

- Fichier de config : [platformio.ini](platformio.ini)

Commandes usuelles :

```bash
platformio run -t upload
platformio device monitor --baud 9600
```

Note : adapte `monitor_port` dans [platformio.ini](platformio.ini) (ex: `COM8`).

## Interface Web

- URL : `http://<ip_de_la_carte>/`
- Rafraîchissement : via polling JavaScript (≈ 1s) sur `GET /api/status`
- Contrôle relais : requêtes HTTP (sans quitter la page)
- Configuration : réseau + MQTT (persistée SPIFFS)

## MQTT

Topics utilisés (référence projet) :

```
waveshare/relay/status
waveshare/input/status
waveshare/sensor/status
waveshare/system/status

waveshare/relay/cmd        (ex: "0:on", "0:off", "ALL:on")
```

## Câblage (pins)

Le document de référence est : [docs/wiring.md](docs/wiring.md)

Rappel :

- W5500 : CS=16, RST=39, SCK=15, MISO=14, MOSI=13
- I2C (TCA9554) : SDA=42, SCL=41
- Entrées : GPIO 4..11
- DHT22 : DATA **GPIO21** (VCC 3.3V, GND)

## Outils (scripts)

- MQTT : [tools/mqtt](tools/mqtt)
  - Configuration : [tools/mqtt/configure_mqtt.py](tools/mqtt/configure_mqtt.py)
- Web/diagnostic HTTP : [tools/web](tools/web)
- Série : [tools/serial](tools/serial)
- Tests scripts : [tools/tests](tools/tests)

## Structure du dépôt

- Firmware : [src](src)
- Documentation : [docs](docs)
- Outils : [tools](tools)
- Archives/anciens essais : [archive](archive)
**Carte**: Waveshare ESP32-S3-ETH-8DI-8RO  
**Framework**: Arduino/PlatformIO
