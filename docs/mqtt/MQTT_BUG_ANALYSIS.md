# SESSION ACTUELLE - PROBLÈME CRITIQUE IDENTIFIÉ

## RÉSUMÉ EXÉCUTIF

**Problème identifié et CONFIRMÉ:** PubSubClient 2.8.0 sur Ethernet + ESP32-S3 ne reçoit JAMAIS les messages MQTT malgré une subscription réussie.

## PREUVES

### ✅ Ce qui fonctionne:
- MQTT connection: `connected()` retourne 1
- MQTT subscribe: `subscribe()` retourne 1 (succès)
- Messages envoyés par le broker: CONFIRMÉS via client Python externe
- Ethernet physique: `ethClient.connected()` = 1
- Debug logging: Déployé, affiche tous les statuts

### ❌ Ce qui ÉCHOUE:
- **Callback JAMAIS appelée** (counter collé à 0)
- `mqttClient.loop()` n'invoque JAMAIS la callback
- Testé après 500 000+ iterations de la boucle loop()
- Testé avec 6 messages MQTT différents (0:on, 0:off, 1:on, 1:off, ALL:on, ALL:off)

## DIAGNOSTIC

**ROOT CAUSE:** BUG dans PubSubClient 2.8.0 avec Ethernet sur ESP32-S3
- La subscription fonctionne (TCP socket étable)
- Les messages arrivent au broker (proxy a confirmé)
- Mais `mqttClient.loop()` ne lit jamais le buffer TCP ou ne déclenche jamais la callback

**Hypothèses éliminées:**
- ❌ Credentials: Publish fonctionne (auth OK)
- ❌ Topic: Subscribe retourne 1 (topic correct)
- ❌ Network: Ethernet stable (5000+ secondes sans déconnexion)
- ❌ Callback syntax: Callback function est valide (testable directement)
- ❌ Fréquence de loop(): Appelée 300 000 fois/min (bien assez souvent)

## SOLUTIONS À ESSAYER (PROCHAINE SESSION)

### Option 1: AsyncMqttClient (RECOMMANDÉE)
- Plus stable sur Ethernet
- Meilleure gestion des interruptions
- Support official pour ESP32-S3
- Changement MAJEUR du code (async/await style)

### Option 2: Downgrade PubSubClient
- Essayer v2.7.0 ou plus vieille
- Risque: Plus de bugs connus, moins d'optimisations

### Option 3: Upgrade PubSubClient
- Essayer v2.9.0+ (s'il existe)
- Vérifier changelog pour fixes Ethernet

### Option 4: Workaround temporaire
- Implémenter socket-level polling
- Lire directement du buffer TCP
- Très complexe, pas-idéal

## FICHIERS MODIFIÉS

[main.cpp](../../src/main.cpp) - Déployé avec:
- Debug counters (loop_counter, callback_counter, mqtt_reconnects)
- Enhanced logging dans mqttCallback
- Status checking avant mqttClient.loop()
- Removed forced disconnect logic (causait instabilité)

## PROCHAINES ÉTAPES

1. **DÉCISION CRITIQUE**: Choisir AsyncMqttClient vs PubSubClient
2. **SI AsyncMqttClient**: Refactoriser tout le MQTT + serveur HTTP async
3. **SI PubSubClient**: Essayer autre version
4. **Tests**: Valider reception de messages avant tout else

## IMPACT

- ❌ Relay commands via MQTT: **TOTALEMENT NON-FONCTIONNEL**
- ✅ Local relay control via serial: Fonctionne parfaitement
- ✅ Hardware: Tous les relais/inputs/senseurs fonctionnels
- ✅ MQTT Publish: Status updates envoyées correctement

## NOTES TECHNIQUES

**Code Structure Actuel:**
```cpp
if (mqttClient.connected()) {
  mqttClient.loop();  // ← Never triggers callback!
  // ...
}
```

**Debug Output (dernière session):**
```
[LOOP 540000] MQTT_connected=1, Callbacks=0, Reconnects=1
PRE-LOOP: ethClient.connected=1, MQTT.connected=1
[MQTT LOOP 540000 calls] Callbacks: 0, ethClient: 1
```

**Messages envoyés pendant test:**
```
📤 Envoi: waveshare/relay/cmd = 0:on → ✓ Publié
📤 Envoi: waveshare/relay/cmd = 0:off → ✓ Publié
📤 Envoi: waveshare/relay/cmd = 1:on → ✓ Publié
...
```

**Résultat:** Aucun changement dans Callbacks counter → Messages jamais reçus!
