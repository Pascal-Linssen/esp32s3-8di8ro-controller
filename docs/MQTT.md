# Documentation MQTT

## Configuration

### Broker MQTT
- **Adresse par défaut** : `192.168.1.100:1883`
- **Client ID** : `ESP32S3_8DI8RO`
- **Authentification** : Désactivée par défaut

### Modification de l'adresse du broker
Dans `main.cpp`, ligne ~15 :
```cpp
IPAddress mqttServer(192, 168, 1, 100); // Changez cette IP
```

## Topics MQTT

### 📊 État du Système
**Topic** : `esp32s3/status`
**Type** : Publication automatique (toutes les 30s)
**Format JSON** :
```json
{
  "ip": "192.168.1.50",
  "uptime": 3600,
  "ethernet": "OK",
  "i2c": "OK"
}
```

### 🔌 Contrôle des Relais
**Topic de commande** : `esp32s3/relay/cmd`
**Type** : Souscription (écoute des commandes)
**Format** : `RELAY:STATE`

#### Exemples de commandes :
```
1:ON          # Allumer relais 1
1:OFF         # Éteindre relais 1
5:ON          # Allumer relais 5
ALL:OFF       # Éteindre tous les relais
ALL:ON        # Allumer tous les relais
```

**Topic d'état** : `esp32s3/relay/state`
**Type** : Publication automatique (à chaque changement)
**Format JSON** :
```json
{
  "relays": [0, 1, 0, 0, 1, 0, 0, 0]
}
```
*(1 = ON, 0 = OFF pour relais 1-8)*

### 📥 État des Entrées
**Topic** : `esp32s3/input/state`
**Type** : Publication automatique (à chaque changement)
**Format JSON** :
```json
{
  "inputs": [1, 0, 1, 1, 0, 0, 1, 0]
}
```
*(1 = HIGH, 0 = LOW pour entrées 1-8)*

### 🌡️ Données des Capteurs
**Topic** : `esp32s3/sensor`
**Type** : Publication automatique (toutes les 30s)
**Format JSON** :
```json
{
  "temperature": 23.5,
  "humidity": 45.2,
  "timestamp": 3600
}
```

## Exemples d'Utilisation

### Home Assistant
```yaml
# configuration.yaml
sensor:
  - platform: mqtt
    name: "ESP32 Temperature"
    state_topic: "esp32s3/sensor"
    value_template: "{{ value_json.temperature }}"
    unit_of_measurement: "°C"

switch:
  - platform: mqtt
    name: "Relay 1"
    command_topic: "esp32s3/relay/cmd"
    state_topic: "esp32s3/relay/state"
    payload_on: "1:ON"
    payload_off: "1:OFF"
    value_template: "{{ value_json.relays[0] }}"
```

### Node-RED
**Contrôler un relais** :
```
[inject] → [change: msg.payload = "1:ON"] → [mqtt out: esp32s3/relay/cmd]
```

**Surveiller les entrées** :
```
[mqtt in: esp32s3/input/state] → [json] → [function: parse inputs] → [debug]
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
    command = f"{relay_num}:{'ON' if state else 'OFF'}"
    client.publish("esp32s3/relay/cmd", command)

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect("192.168.1.100", 1883, 60)

# Allumer relais 1
set_relay(client, 1, True)

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
# Écouter tous les topics
mosquitto_sub -h 192.168.1.100 -t "esp32s3/+"

# Contrôler relais 1
mosquitto_pub -h 192.168.1.100 -t "esp32s3/relay/cmd" -m "1:ON"
mosquitto_pub -h 192.168.1.100 -t "esp32s3/relay/cmd" -m "1:OFF"

# Éteindre tous les relais
mosquitto_pub -h 192.168.1.100 -t "esp32s3/relay/cmd" -m "ALL:OFF"
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