# Guide Modbus TCP - ESP32-S3-ETH-8DI-8RO

## 🏭 Configuration Modbus TCP

L'ESP32-S3-ETH-8DI-8RO fonctionne comme **serveur Modbus TCP** via Ethernet W5500:

- **IP**: 192.168.1.50 (configurable dans main.cpp)
- **Port**: 502 (standard Modbus TCP)
- **Protocole**: Modbus TCP/IP over Ethernet
- **Bibliothèque**: modbus-esp8266 v4.1.0 avec ModbusEthernet
- **Unit ID**: 1 (par défaut)

## 📊 Plan de Registres

### Coils (Relais) - Lecture/Écriture
| Adresse | Description | Type | Accès |
|---------|-------------|------|-------|
| 0 | Relais 1 | Coil | R/W |
| 1 | Relais 2 | Coil | R/W |
| 2 | Relais 3 | Coil | R/W |
| 3 | Relais 4 | Coil | R/W |
| 4 | Relais 5 | Coil | R/W |
| 5 | Relais 6 | Coil | R/W |
| 6 | Relais 7 | Coil | R/W |
| 7 | Relais 8 | Coil | R/W |

### Discrete Inputs (Entrées) - Lecture Seule
| Adresse | Description | Type | Accès |
|---------|-------------|------|-------|
| 10000 | Entrée 1 | Input | R |
| 10001 | Entrée 2 | Input | R |
| 10002 | Entrée 3 | Input | R |
| 10003 | Entrée 4 | Input | R |
| 10004 | Entrée 5 | Input | R |
| 10005 | Entrée 6 | Input | R |
| 10006 | Entrée 7 | Input | R |
| 10007 | Entrée 8 | Input | R |

### Input Registers (Capteurs) - Lecture Seule
| Adresse | Description | Unité | Facteur | Accès |
|---------|-------------|-------|---------|-------|
| 30000 | Température | °C | x10 | R |
| 30001 | Humidité | % | x10 | R |

## 💡 Exemples d'Utilisation

### Python avec pymodbus
```python
from pymodbus.client.sync import ModbusTcpClient

# Connexion
client = ModbusTcpClient('192.168.1.50', port=502)

# Activer relais 1
client.write_coil(0, True)

# Désactiver relais 1
client.write_coil(0, False)

# Lire toutes les entrées
inputs = client.read_discrete_inputs(10000, 8)
print("Entrées:", inputs.bits[:8])

# Lire température et humidité
registers = client.read_input_registers(30000, 2)
temperature = registers.registers[0] / 10.0
humidity = registers.registers[1] / 10.0
print(f"Temp: {temperature}°C, Humidité: {humidity}%")

client.close()
```

### Node-RED
```json
{
  "id": "modbus-read",
  "type": "modbus-read",
  "name": "Lire Entrées",
  "topic": "",
  "showStatusActivities": false,
  "showErrors": false,
  "unitid": "1",
  "dataType": "DiscreteInput",
  "adr": "10000",
  "quantity": "8",
  "rate": "1000",
  "server": "modbus-server"
}
```

### SCADA (Wonderware, FactoryTalk, etc.)
- **Driver**: Modbus TCP/IP
- **IP**: 192.168.1.50
- **Port**: 502
- **Unit ID**: 1
- **Scan Rate**: 1000ms (recommandé)

## 🔧 Configuration Avancée

### Modifier l'IP (dans le code)
```cpp
IPAddress ip(192, 168, 1, 50);  // Changez ici
```

### Ajouter des Registres Personnalisés
```cpp
// Holding Registers pour paramètres
#define MODBUS_PARAM_BASE 40000
mb.addHreg(MODBUS_PARAM_BASE);     // Paramètre 1
mb.addHreg(MODBUS_PARAM_BASE + 1); // Paramètre 2
```

## 🚨 Diagnostics

### Commandes Série de Test
```
status     # Vérifier l'état Modbus
modbus     # Afficher la configuration complète
```

### Résolution de Problèmes

**Pas de connexion Modbus:**
1. Vérifier l'état Ethernet : `status`
2. Vérifier l'IP du client
3. Vérifier le port 502

**Relais ne répondent pas:**
1. Vérifier TCA9554 : `scan`
2. Tester manuellement : `relay 1 on`
3. Vérifier les registres Modbus

**Valeurs capteurs incorrectes:**
1. Vérifier DHT22 dans `status`
2. Les valeurs sont multipliées par 10

## 📋 Fonctions Modbus Supportées

- **01 - Read Coils**: ✅ Lecture relais
- **02 - Read Discrete Inputs**: ✅ Lecture entrées
- **04 - Read Input Registers**: ✅ Lecture capteurs
- **05 - Write Single Coil**: ✅ Contrôle relais
- **15 - Write Multiple Coils**: ✅ Contrôle multiple

## 🔒 Sécurité

⚠️ **Attention**: Modbus TCP n'a pas d'authentification native
- Utiliser un firewall pour limiter l'accès
- Considérer un VPN pour l'accès distant
- Surveiller les connexions dans les logs

## 🎯 Intégration Systèmes

### Home Assistant
```yaml
modbus:
  - name: esp32_controller
    type: tcp
    host: 192.168.1.50
    port: 502
    
switch:
  - platform: modbus
    registers:
      - name: "Relais 1"
        hub: esp32_controller
        register: 0
        command_on: 1
        command_off: 0
```

### OpenHAB
```
Thing modbus:tcp:esp32 "ESP32 Controller" @ "Automation" [
    host="192.168.1.50",
    port=502,
    id=1
]
```

Cette configuration Modbus TCP permet l'intégration complète dans tous les systèmes d'automatisation industriels standard.