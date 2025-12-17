#!/usr/bin/env python3
"""
Envoie les mêmes commandes que le test diagnostique
mais en attendant plus longtemps entre chaque
"""

import paho.mqtt.client as mqtt
import time
import json
from datetime import datetime

import os

BROKER = os.getenv("MQTT_HOST", "192.168.1.200")
PORT = int(os.getenv("MQTT_PORT", "1883"))
USERNAME = os.getenv("MQTT_USERNAME", "")
PASSWORD = os.getenv("MQTT_PASSWORD", "")

relay_history = []

def on_connect(client, userdata, flags, reason_code):
    if reason_code == 0:
        print("✅ Connecté au broker")
        client.subscribe("#")
    else:
        print(f"❌ Erreur connexion: {reason_code}")

def on_message(client, userdata, msg):
    global relay_history
    
    if "relay/status" in msg.topic:
        try:
            data = json.loads(msg.payload.decode())
            relay_history.append({
                'time': datetime.now().strftime('%H:%M:%S'),
                'relays': data
            })
            # Afficher les changements
            if len(relay_history) > 1:
                prev = relay_history[-2]['relays']
                curr = relay_history[-1]['relays']
                for i in range(8):
                    if prev.get(f'relay_{i}') != curr.get(f'relay_{i}'):
                        print(f"\n🔄 CHANGEMENT DÉTECTÉ!")
                        print(f"   [{datetime.now().strftime('%H:%M:%S')}] relay_{i}: {prev.get(f'relay_{i}')} → {curr.get(f'relay_{i}')}")
        except:
            pass

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
if USERNAME:
    client.username_pw_set(USERNAME, PASSWORD)
client.connect(BROKER, PORT, keepalive=60)
client.loop_start()

print("\n" + "="*60)
print("TEST DÉTAILLÉ DES RELAIS")
print("="*60)

time.sleep(2)

# Test chaque relai individuellement
for relay_num in range(3):
    print(f"\n[Test relai {relay_num}]")
    
    # Allumer
    print(f"  📤 Envoi: {relay_num}:on")
    client.publish("waveshare/relay/cmd", f"{relay_num}:on", qos=1)
    
    print(f"  ⏳ Attente 3s...")
    for i in range(3):
        time.sleep(1)
        if relay_history:
            current = relay_history[-1]['relays'].get(f'relay_{relay_num}')
            print(f"     [{i+1}s] relay_{relay_num} = {current}")
    
    # Éteindre
    print(f"  📤 Envoi: {relay_num}:off")
    client.publish("waveshare/relay/cmd", f"{relay_num}:off", qos=1)
    
    print(f"  ⏳ Attente 3s...")
    for i in range(3):
        time.sleep(1)
        if relay_history:
            current = relay_history[-1]['relays'].get(f'relay_{relay_num}')
            print(f"     [{i+1}s] relay_{relay_num} = {current}")

print(f"\n" + "="*60)
print("HISTORIQUE COMPLET:")
print("="*60)
for entry in relay_history[-10:]:
    print(f"[{entry['time']}] {entry['relays']}")

print("\n" + "="*60)

client.loop_stop()
client.disconnect()
