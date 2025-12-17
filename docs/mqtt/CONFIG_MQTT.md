# 🔧 Configuration MQTT - Vue d'ensemble

## État actuel (v1.6)

Le système ESP32-S3-ETH-8DI-8RO dispose maintenant d'une **configuration MQTT persistante** qui peut être modifiée **sans recompiler le firmware**.

### Où est la configuration stockée?

- **Stockage**: Partition SPIFFS (Flash du ESP32)
- **Fichier**: `/config.json`
- **Persistance**: Sauvegardée lors du redémarrage

### Comment modifier les paramètres MQTT?

Trois méthodes:

#### 1️⃣ Via CLI Python (Recommandé)

```bash
python tools/mqtt/configure_mqtt.py
```

Menu interactif pour:
- ✏️ Modifier adresse broker et port
- ✏️ Modifier credentials (user/password)
- ✏️ Modifier les topics MQTT
- ✏️ Réinitialiser aux défauts

#### 2️⃣ Via editsérielle CLI (En cours d'implémentation)

Commandes sur le port série (COM4 @ 9600):
```
config show              # Affiche config actuelle
config broker 192.168.1.200
config port 1883
config user nomutilisateur
config password monmdp
config topic-relay-cmd home/esp32/relay/cmd
```

#### 3️⃣ Via Interface Web (Disponible)

Accès sur: `http://<ip_de_la_carte>/` (section configuration)

API:
- `GET /api/config`
- `POST /api/config`

## Configuration par défaut

```json
{
  "broker_ip": "192.168.1.200",
  "broker_port": 1883,
  "username": "<mqtt_username>",
  "password": "<mqtt_password>",
  "topic_relay_cmd": "home/esp32/relay/cmd",
  "topic_relay_status": "home/esp32/relay/status",
  "topic_input_status": "home/esp32/input/status",
  "topic_sensor_status": "home/esp32/sensor/status",
  "topic_system_status": "home/esp32/system/status"
}
```

## Workflow de modification

```
┌─────────────────────────────────────┐
│  Modifier avec configure_mqtt.py    │
│  ou mise à jour manuelle JSON       │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  JSON sauvegardé en local           │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  À l'étape suivante:                │
│  - Télécharger via Web (À faire)    │
│  - Uploader via CLI (À faire)       │
│  - Ou via sérielle CLI (À faire)    │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  ESP32 redémarre                    │
│  Charge /config.json depuis SPIFFS  │
│  Reconnecte à MQTT avec nva config  │
└─────────────────────────────────────┘
```

## Prochaines étapes (optionnel)

- CLI sérielle pour éditer la config sans PC
- `/api/reconnect` dédié (si tu veux reconnecter MQTT sans redémarrage)

## Limitations actuelles

- La méthode CLI Python reste utile pour automatiser des déploiements, mais n’est plus obligatoire

## Comment aider au développement?

Le système SPIFFS est prêt. Prochaine étape: **Implémenter serveur HTTP simple** sur EthernetClient pour:
1. Servir page web statique `/config`
2. API REST `/api/config` (GET/POST)
3. API `/api/reconnect` pour reconnecter MQTT

## Exemple: Modifier broker via Python

```bash
python tools/mqtt/configure_mqtt.py
```

```
📡 CONFIGURATION MQTT ACTUELLE
==============================================================

🔗 Broker: 192.168.1.200:1883
👤 Utilisateur: pascal
🔐 Mot de passe: ***
📨 Topics:
   Relais (CMD):    home/esp32/relay/cmd
   Relais (STATUS): home/esp32/relay/status
   Entrées:         home/esp32/input/status
   Capteurs:        home/esp32/sensor/status
   Système:         home/esp32/system/status

Choisissez une option:
1. Modifier adresse broker
2. Modifier port broker
3. Modifier utilisateur
4. Modifier mot de passe
5. Modifier topics
6. Réinitialiser aux défauts
0. Quitter
```

## Notes pour l'implémentation future

- Le header `web_config.h` contient les fonctions:
  - `initSPIFFS()` - Initialize flash
  - `loadMQTTConfig()` - Load from /config.json
  - `saveMQTTConfig()` - Save to /config.json
  - `handleWebServer()` - Stub pour HTTP (À implémenter)

- Utilise `ArduinoJson` (v6.21.4) pour sérialiser/désérialiser
- Tous les paramètres sont stockés en SPIFFS automatiquement
