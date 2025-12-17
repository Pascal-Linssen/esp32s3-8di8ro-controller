# Configuration des Pins - ESP32-S3-ETH-8DI-8RO

## ✅ Configuration Validée (Waveshare Officiel)

### TCA9554 I2C (Relais) - **FONCTIONNEL**
```
SDA: Pin 42 ⚡ (Confirmé par démo Waveshare)
SCL: Pin 41 ⚡ (Confirmé par démo Waveshare)
Adresse I2C: 0x20
```

### Entrées Digitales - **FONCTIONNEL**
```
INPUT_1: Pin 4
INPUT_2: Pin 5  
INPUT_3: Pin 6
INPUT_4: Pin 7
INPUT_5: Pin 8
INPUT_6: Pin 9
INPUT_7: Pin 10
INPUT_8: Pin 11
```
*Configuration avec pull-up interne*

### Ethernet W5500 - **CONFIGURÉ**
```
CS:   Pin 16
RST:  Pin 39
SCK:  Pin 15
MISO: Pin 14
MOSI: Pin 13
```
*Pins selon schéma Waveshare officiel*

### DHT22 Température/Humidité - **CONFIGURÉ**
```
Data: Pin 21
VCC:  3.3V
GND:  GND
```

## 🔧 Connexions Externes

### Relais (TCA9554)
- **Sortie**: 8 relais contrôlés via I2C
- **Type**: NO/NC disponibles
- **Courant max**: 10A 250V AC / 30V DC

### Entrées Digitales
- **Type**: Optocouplers bidirectionnels
- **Tension**: 5V-36V
- **Isolation**: Galvanique

### DHT22 (Si utilisé)
```
DHT22   ->  ESP32-S3
VCC     ->  3.3V
Data    ->  Pin 21
GND     ->  GND
```

## 🚀 Validation

### Test I2C TCA9554
```
scan
# Résultat attendu: Périphérique trouvé à l'adresse 0x20
```

### Test Relais
```
testio
# ou
relay 1 on
relay 1 off
```

### Test Entrées
```
status
# Vérifier "Entrées: 1:X 2:X ..." dans l'affichage
```

## 🎯 Notes Importantes

1. **Pins I2C**: Utilisation obligatoire de SDA=42, SCL=41 (pins Waveshare officiels)
2. **Ethernet**: Nécessite connexion physique du câble pour validation
3. **Alimentation**: Carte supporte 7-36V sur bornier d'alimentation
4. **Isolation**: Relais et entrées sont galvaniquement isolés
5. **Interface**: Commandes série à 9600 bauds avec USB CDC activé
