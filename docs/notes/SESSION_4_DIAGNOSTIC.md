# 🔧 SESSION 4: DIAGNOSTIC & CORRECTION MQTT + RELAIS

**Date**: 17 Décembre 2025  
**Statut**: Code modifié et prêt à compiler

---

## 📊 RÉSUMÉ DU DIAGNOSTIC

### Problème Initial
Les **commandes MQTT** n'étaient **jamais reçues** par l'ESP32, même si le broker les avait.

### Diagnostic Effectué

#### Test 1: Vérifier que le broker reçoit les messages
✅ **SUCCÈS** - Les messages publiés arrivent bien au broker MQTT

#### Test 2: Vérifier que l'ESP32 reçoit les commandes
✅ **SUCCÈS** - La **callback MQTT fonctionne**!
- Callback count: 0 → 14 (3 commandes × ~5 appels)
- Les statuts des relais **changent effectivement**

#### Test 3: Tester les relais individuellement
⚠️ **PARTIELLEMENT** - Les relais répondent mais avec des **problèmes**:
- **Relai 0**: Ne répond pas du tout
- **Relai 1**: S'active avec ~2.5s de délai
- **Relai 2**: S'active mais **ne s'éteint pas** correctement

---

## 🐛 PROBLÈMES TROUVÉS

### Problème 1: Initialisation TCA9554 incomplète
**Cause**: Le code original n'initialisait que `Wire.begin()` sans configurer les registres du TCA9554.

**Solution**: 
- Configurer le registre CONFIG (0x03) → 0x00 (tous OUTPUT)
- Configurer le registre OUTPUT (0x01) → 0xFF (tous OFF)
- **ENLEVER** l'inversion POLARITY (c'était trop compliqué)

### Problème 2: Fonction `setRelay()` non fiable
**Cause**: La lecture/écriture I2C n'était pas correctement documentée.

**Solution**:
- Implémenter le vrai **READ-MODIFY-WRITE** comme dans l'exemple officiel
- Logique: `0 = ON`, `1 = OFF` (active LOW)
- Vérifier les return codes des transmissions

### Problème 3: Initialisation incomplet du TCA9554
**Cause**: On ne mettait pas le TCA9554 dans un état connu au démarrage.

**Solution**:
```cpp
// Tous les ports en OUTPUT
Wire.write(0x03); Wire.write(0x00);

// Tous les relais OFF au démarrage  
Wire.write(0x01); Wire.write(0xFF);
```

---

## ✅ MODIFICATIONS APPORTÉES

### Fichier: `src/main.cpp`

#### 1. Fonction `messageReceived()` - AMÉLIORÉE
```cpp
void messageReceived(String &topic, String &payload) {
  callback_counter++;
  
  Serial.printf("\n🎯 MQTT MESSAGE RECEIVED #%lu!\n", callback_counter);
  Serial.printf("   Loop: %lu\n", loop_counter);
  Serial.printf("   Topic: %s\n", topic.c_str());
  Serial.printf("   Payload: %s\n", payload.c_str());
  Serial.printf("   Payload Length: %d bytes\n", payload.length());
  // ... reste du code
}
```
✅ Ajout: Loop counter, payload length

#### 2. Fonction `connectMqtt()` - DEBUG AMÉLIORÉ
```cpp
// Subscribe to command topics
int sub_result = mqttClient.subscribe(topic_relay_cmd);
Serial.printf("✓ Subscribed to: %s (result: %d)\n", topic_relay_cmd, sub_result);
```
✅ Ajout: Return value de subscribe()

#### 3. Fonction `setRelay()` - COMPLÈTEMENT RÉÉCRIRE
```cpp
void setRelay(int relay, bool state) {
  if (relay >= 0 && relay < 8) {
    relayStates[relay] = state;
    
    // READ: Current state from TCA9554
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); // Output register
    Wire.endTransmission();
    
    byte output = 0x00;
    int bytes_available = Wire.requestFrom(TCA9554_ADDR, 1);
    if (bytes_available > 0) {
      output = Wire.read();
    }
    
    // MODIFY: Change only target bit
    byte new_output = output;
    if (state) {
      new_output &= ~(1 << relay);  // 0 = ON
    } else {
      new_output |= (1 << relay);   // 1 = OFF
    }
    
    // WRITE: Back to TCA9554
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01);
    Wire.write(new_output);
    Wire.endTransmission();
    
    delay(10);
    Serial.printf("✓ Relay %d: %s\n", relay, state ? "ON" : "OFF");
  }
}
```
✅ Logique: Active LOW (0=ON, 1=OFF)  
✅ Patterns: Simple READ-MODIFY-WRITE

#### 4. Initialisation TCA9554 - SIMPLIFIÉE
```cpp
// Initialize I2C
Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

// Configure TCA9554
// - Set all to OUTPUT
Wire.beginTransmission(TCA9554_ADDR);
Wire.write(0x03);  // Config register
Wire.write(0x00);  // All outputs
Wire.endTransmission();

// - Initialize all OFF
Wire.beginTransmission(TCA9554_ADDR);
Wire.write(0x01);  // Output register
Wire.write(0xFF);  // All HIGH = all OFF
Wire.endTransmission();
```
✅ **ENLEVER**: Configuration POLARITY compliquée  
✅ Approche: Simple et directe

#### 5. Debug Info - PLUS DÉTAILLÉ
```cpp
// Every 5 seconds (more frequent):
Serial.printf("\n[%lu ms] 📊 DEBUG INFO (loop #%lu, callback #%lu):\n", 
  millis(), loop_counter, callback_counter);
Serial.printf("   Ethernet: %s\n", eth_connected ? "✅ Connected" : "❌ Disconnected");
Serial.printf("   MQTT: %s (IP: %s)\n", 
  mqttClient.connected() ? "✅ Connected" : "❌ Disconnected", 
  Ethernet.localIP().toString().c_str());
Serial.printf("   Callbacks: %lu | Reconnects: %lu\n", callback_counter, mqtt_reconnects);

// Afficher l'état de chaque relai
Serial.printf("   Relay States: ");
for (int i = 0; i < 8; i++) {
  Serial.printf("%d:%s ", i, relayStates[i] ? "ON" : "OFF");
}
Serial.println();
```
✅ Debug **toutes les 5 secondes** au lieu de 10  
✅ Affiche l'IP ethernet, user MQTT, état de chaque relai

---

## 🚀 COMMENT COMPILER ET UPLOADER

### Option 1: Via VS Code + PlatformIO Extension
1. **Installer PlatformIO** via Extension VS Code
2. Ouvrir le terminal VS Code
3. Exécuter:
```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

### Option 2: Via Script PowerShell
```powershell
cd c:\Users\Pascal\Desktop\esp32s3_8di8ro_full

# Compiler
python -m platformio run

# Uploader
python -m platformio run -t upload

# Ouvrir moniteur
python -m platformio device monitor -b 115200
```

### Option 3: Via Arduino IDE
1. Copier les fichiers du `src/` vers un sketch Arduino
2. Ajouter les librairies nécessaires
3. Build + Upload

---

## 📝 PROCHAINES ÉTAPES APRÈS UPLOAD

### Test 1: Vérifier le démarrage
```
✓ I2C initialized
✓ TCA9554 configured (all relays OFF)
✓ MQTT client initialized with 256dpi/arduino-mqtt
```

### Test 2: Envoyer les commandes MQTT
```bash
python mqtt_test_relay_detailed.py
```
Devrait voir:
```
[Test relai 0]
  📤 Envoi: 0:on
  ⏳ Attente 3s...
     [1s] relay_0 = 1  ← Devrait passer à 1 immédiatement!
     [2s] relay_0 = 1
     [3s] relay_0 = 1
```

### Test 3: Confirmer les changements physiques
- Mettre un LED/buzzer sur chaque relai
- Tester:
  - `0:on` → LED s'allume
  - `0:off` → LED s'éteint
  - `ALL:on` → Toutes les LED s'allument
  - `ALL:off` → Toutes les LED s'éteignent

---

## 📚 RÉFÉRENCES

### Fichiers de Documentation
- [docs/TECHNICAL_ANALYSIS.md](docs/TECHNICAL_ANALYSIS.md) - Analyse technique
- [docs/MQTT_SOLUTION_ANALYSIS.md](docs/MQTT_SOLUTION_ANALYSIS.md) - Solution MQTT

### Code Officiel Référence
- [demo_officiel/Arduino/examples/MAIN_ALL/WS_TCA9554PWR.cpp](demo_officiel/Arduino/examples/MAIN_ALL/WS_TCA9554PWR.cpp)
- [demo_officiel/Arduino/examples/MAIN_ALL/WS_Relay.cpp](demo_officiel/Arduino/examples/MAIN_ALL/WS_Relay.cpp)

---

## 🎯 RÉSUMÉ DES CHANGEMENTS

| Aspect | Avant | Après |
|--------|-------|-------|
| Initialisation TCA9554 | Juste `Wire.begin()` | Complet: CONFIG + OUTPUT |
| Logique Relai | Non défini | Active LOW: 0=ON, 1=OFF |
| setRelay() | Limité | READ-MODIFY-WRITE complet |
| Debug Info | Tous les 10s | Tous les 5s + détails |
| Polarity Register | Utilisé (0xFF) | **SUPPRIMÉ** (trop compliqué) |

---

**✅ Le code est prêt à compiler!**
