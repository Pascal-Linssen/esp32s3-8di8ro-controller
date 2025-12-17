#!/usr/bin/env python3
"""
DIAGNOSTIC FINAL - Confirme que tout fonctionne
"""

import paho.mqtt.client as mqtt
import time
import json
from datetime import datetime

BROKER = "192.168.1.200"
PORT = 1883
USERNAME = "pascal"
PASSWORD = "123456"

last_system_status = {}
relay_changes = []

def on_connect(client, userdata, flags, reason_code):
    if reason_code == 0:
        print("✅ Connecté au broker")
        client.subscribe("#")
    else:
        print(f"❌ Erreur connexion: {reason_code}")

def on_message(client, userdata, msg):
    global last_system_status, relay_changes
    
    topic = msg.topic
    payload = msg.payload.decode()
    
    # Capturer les changements de relais
    if "relay/status" in topic:
        try:
            data = json.loads(payload)
            relay_changes.append({
                'time': datetime.now().strftime('%H:%M:%S'),
                'relays': data
            })
            # Garder seulement les 5 derniers
            if len(relay_changes) > 5:
                relay_changes.pop(0)
        except:
            pass
    
    # Capturer le statut système
    if "system/status" in topic:
        try:
            last_system_status = json.loads(payload)
        except:
            pass

# Connexion
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.username_pw_set(USERNAME, PASSWORD)
client.connect(BROKER, PORT, keepalive=60)
client.loop_start()

print("\n" + "="*60)
print("DIAGNOSTIC FINAL MQTT")
print("="*60)

time.sleep(2)  # Laisser la connexion se faire

# Attendre un statut système initial
print("\n⏳ Attente du statut système initial...")
for i in range(5):
    if last_system_status:
        print(f"✅ Reçu après {i+1}s")
        break
    time.sleep(1)

# Afficher l'état initial
if last_system_status:
    print(f"\n📊 État initial:")
    print(f"   Uptime: {last_system_status.get('uptime_ms', 0)}ms")
    print(f"   Loop count: {last_system_status.get('loop_count', 0)}")
    print(f"   Callback count: {last_system_status.get('callback_count', 0)}")
    print(f"   MQTT reconnects: {last_system_status.get('mqtt_reconnects', 0)}")

# Envoyer des commandes
print(f"\n📤 Envoi des commandes test...")
print(f"   Commande 1: 0:on")
client.publish("home/esp32/relay/cmd", "0:on", qos=1)
time.sleep(1)

print(f"   Commande 2: 1:on")
client.publish("home/esp32/relay/cmd", "1:on", qos=1)
time.sleep(1)

print(f"   Commande 3: 0:off")
client.publish("home/esp32/relay/cmd", "0:off", qos=1)
time.sleep(2)

# Attendre le statut final
print(f"\n⏳ Attente du statut final...")
time.sleep(2)

# Afficher les résultats
print(f"\n" + "="*60)
print("RÉSULTATS:")
print("="*60)

if last_system_status:
    print(f"\n📈 Statut final:")
    print(f"   Uptime: {last_system_status.get('uptime_ms', 0)}ms")
    print(f"   Loop count: {last_system_status.get('loop_count', 0)}")
    print(f"   Callback count: {last_system_status.get('callback_count', 0)}")
    print(f"   MQTT reconnects: {last_system_status.get('mqtt_reconnects', 0)}")
    
    if last_system_status.get('callback_count', 0) >= 3:
        print(f"\n✅ SUCCÈS! La callback a été appelée au moins 3 fois!")
    else:
        print(f"\n⚠️ Attention: La callback n'a été appelée que {last_system_status.get('callback_count', 0)} fois")

if relay_changes:
    print(f"\n🔄 Changements de relais détectés:")
    for change in relay_changes:
        print(f"   [{change['time']}] {change['relays']}")

print("\n" + "="*60)

client.loop_stop()
client.disconnect()
