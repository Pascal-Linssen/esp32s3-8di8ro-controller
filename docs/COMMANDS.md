# Guide des Commandes

## Commandes de Base

### État du Système
```
status
```
Affiche l'état complet : Ethernet, I2C, température, entrées, relais

### Aide
```
help
```
Liste toutes les commandes disponibles

## Diagnostic I2C

### Scan I2C
```
scan
```
Recherche tous les périphériques I2C (TCA9554 doit être à 0x20)

### Test des Pins I2C
```
testpins
```
Teste différentes combinaisons de pins SDA/SCL pour trouver la configuration optimale

## Contrôle MQTT

### Format JSON (recommandé)
Topic: `esp32s3/relay/cmd`
```json
{"relay": 1, "state": "on"}
{"relay": 1, "state": "off"}
```

### Format Simple
Topic: `esp32s3/relay/cmd`
```
1:ON
1:OFF
ALL:OFF
```

### Topics de Status
- `esp32s3/sensor` - Température, humidité, entrées
- `esp32s3/relay/status` - État de tous les relais
- `esp32s3/relay/1/status` - État relais individuel

## Contrôle des Relais

### Activer un Relais
```
relay 1 on
relay 2 on
...
relay 8 on
```

### Désactiver un Relais
```
relay 1 on

relay 2 off
...
relay 8 off
```

## Tests Hardware

### Test Entrées/Sorties
```
testio
```
- Affiche l'état de toutes les entrées digitales (0 = inactif, 1 = activé)
- Teste séquentiellement tous les relais (si TCA9554 OK)

### Test Diagnostic MQTT
```
mqtttest
```
- Affiche l'état de connexion MQTT
- Vérifie broker et souscriptions
- Code d'erreur client MQTT

### Configuration des Pins
```
pins
```
Affiche la configuration complète de tous les pins :
- Ethernet W5500
- Entrées digitales
- I2C TCA9554
- DHT22

## Exemples d'Utilisation

### Test Rapide du Système
```
status
scan
testio
```

### Contrôle Manuel des Relais
```
relay 1 on
# Attendre...
relay 1 off
relay 2 on
relay 2 off
```

### Diagnostic Complet
```
pins
scan
testpins
testio
status
```

## Messages d'État

### TCA9554 I2C: ✓ OK
Relais fonctionnels, pins I2C correctement configurés

### TCA9554 I2C: ✗ ERREUR  
Problème de communication I2C, vérifier pins SDA/SCL

### Ethernet W5500: ✓ OK
Connexion Ethernet active avec IP affichée

### Ethernet W5500: ✗ ERREUR
Problème hardware W5500 ou câble non connecté

## Format des Commandes

- **Sensibilité** : Les commandes ne sont pas sensibles à la casse
- **Format relais** : `relay [1-8] [on/off]`
- **Espaces** : Respecter les espaces dans les commandes
- **Validation** : Le système valide les paramètres et affiche des erreurs explicites