# Documentation MQTT

Cette documentation décrit les topics et formats utilisés par le firmware actuel.

## Configuration

### Broker MQTT
- **Adresse** : `192.168.1.200:1883`
- **Client ID** : `ESP32S3_8DI8RO`
- **Authentification** :
  - **Login** : `<mqtt_username>`
  - **Password** : `<mqtt_password>`

### Modification de la configuration
La configuration MQTT est persistée dans SPIFFS (`/config.json`) et peut être modifiée via l’interface Web ou via les scripts dans `tools/mqtt/`.

## Topics MQTT

### 📊 État du Système
**Topic** : `waveshare/system/status`
**Type** : Publication automatique (toutes les 30s)
**Format JSON** :
```json
{
  "ip": "192.168.1.50",
  "mqtt": "connected",
  "uptime": 3600
}
```

### 🔌 Contrôle des Relais
**Topic de commande** : `waveshare/relay/cmd`
**Type** : Souscription (écoute des commandes)
**Format** : `RELAY:STATE`

Remarque: les relais sont indexés de `0` à `7`.

#### Exemples de commandes :
```
0:on          # Allumer relais 0
0:off         # Éteindre relais 0
5:on          # Allumer relais 5
ALL:off       # Éteindre tous les relais
ALL:on        # Allumer tous les relais
```

**Topic d'état** : `waveshare/relay/status`
**Type** : Publication automatique (à chaque changement)
**Format JSON** (tableau, index 0..7) :
```json
[0, 1, 0, 0, 1, 0, 0, 0]
```
*(1 = ON, 0 = OFF)*

### 📥 État des Entrées
**Topic** : `waveshare/input/status`
**Type** : Publication automatique (à chaque changement)
**Format JSON** (tableau, index 0..7) :
```json
[1, 0, 1, 1, 0, 0, 1, 0]
```

Les entrées sont en **logique active-bas** (INPUT_PULLUP):
- `1` = **ACTIVE** (niveau bas / 0V)
- `0` = **INACTIVE** (niveau haut / 3.3V)

### 🌡️ Données des Capteurs
**Topic** : `waveshare/sensor/status`
**Type** : Publication automatique (toutes les 30s)
**Format JSON** :
```json
{
  "temperature": 23.5,
  "humidity": 45.2
}
```

## Exemples d'Utilisation

### Home Assistant
```yaml
# configuration.yaml
sensor:
  - platform: mqtt
    name: "ESP32 Temperature"
    state_topic: "waveshare/sensor/status"
    value_template: "{{ value_json.temperature }}"
    unit_of_measurement: "°C"

switch:
  - platform: mqtt
    name: "Relay 0"
    command_topic: "waveshare/relay/cmd"
    state_topic: "waveshare/relay/status"
    payload_on: "0:on"
    payload_off: "0:off"
    value_template: "{{ value_json[0] }}"
```

### Node-RED
**Contrôler un relais** :
```
[inject] → [change: msg.payload = "0:on"] → [mqtt out: waveshare/relay/cmd]
```

**Surveiller les entrées** :
```
[mqtt in: waveshare/input/status] → [json] → [function: parse inputs] → [debug]
```

### Python Script
```python
import paho.mqtt.client as mqtt
import json

def on_connect(client, userdata, flags, rc):
    print(f"Connecté avec code {rc}")
    client.subscribe("esp32s3/+")

def on_message(client, userdata, msg):
    topic = msg.topic
    payload = json.loads(msg.payload.decode())
    print(f"{topic}: {payload}")

# Contrôler relais
def set_relay(client, relay_num, state):
  command = f"{relay_num}:{'on' if state else 'off'}"
  client.publish("waveshare/relay/cmd", command)

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect("192.168.1.200", 1883, 60)

# Allumer relais 1
set_relay(client, 0, True)

client.loop_forever()
```

## Test avec Mosquitto

### Installation
```bash
# Ubuntu/Debian
sudo apt install mosquitto-clients

# Windows
# Télécharger depuis https://mosquitto.org/download/
```

### Commandes de test
```bash
# Écouter tous les topics (avec authentification)
mosquitto_sub -h 192.168.1.200 -u <mqtt_username> -P <mqtt_password> -t "esp32s3/+"

# Contrôler relais 1 (avec authentification)
mosquitto_pub -h 192.168.1.200 -u <mqtt_username> -P <mqtt_password> -t "esp32s3/relay/cmd" -m "1:ON"
mosquitto_pub -h 192.168.1.200 -u <mqtt_username> -P <mqtt_password> -t "esp32s3/relay/cmd" -m "1:OFF"

# Éteindre tous les relais
mosquitto_pub -h 192.168.1.200 -u <mqtt_username> -P <mqtt_password> -t "esp32s3/relay/cmd" -m "ALL:OFF"
```

## Dépannage

### Connexion MQTT échoue
1. Vérifier l'adresse IP du broker
2. Vérifier que le broker MQTT est démarré
3. Vérifier les credentials si authentification activée
4. Regarder les logs série pour les codes d'erreur

### Messages non reçus
1. Vérifier la souscription aux topics
2. Vérifier le format des messages (respecter la casse)
3. Utiliser `mosquitto_sub` pour tester la réception

### Performance
- Publication automatique limitée à 30s pour éviter le spam
- Reconnexion MQTT limitée à une tentative toutes les 5s
- Utiliser QoS 0 par défaut pour optimiser les performances