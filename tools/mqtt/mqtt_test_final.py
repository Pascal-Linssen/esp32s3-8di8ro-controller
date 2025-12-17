#!/usr/bin/env python3
"""
Test final: tous les relais avec vérification initiale
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
initial_state = {}

def on_connect(client, userdata, flags, reason_code):
    if reason_code == 0:
        print("✅ Connecté au broker\n")
        client.subscribe("#")
    else:
        print(f"❌ Erreur connexion: {reason_code}")

def on_message(client, userdata, msg):
    global relay_history, initial_state
    
    if "relay/status" in msg.topic:
        try:
            data = json.loads(msg.payload.decode())
            
            # Première réception = état initial
            if not initial_state:
                initial_state = data.copy()
                print("📊 ÉTAT INITIAL AU DÉMARRAGE:")
                for i in range(8):
                    relay = f'relay_{i}'
                    state = data.get(relay, 0)
                    print(f"   relay_{i}: {'🟢 ON' if state else '⚫ OFF'}")
                return
            
            # Détecter les changements
            relay_history.append({
                'time': datetime.now().strftime('%H:%M:%S'),
                'relays': data
            })
        except:
            pass

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
if USERNAME:
    client.username_pw_set(USERNAME, PASSWORD)
client.connect(BROKER, PORT, keepalive=60)
client.loop_start()

print("="*60)
print("TEST FINAL - TOUS LES RELAIS")
print("="*60 + "\n")

time.sleep(2)  # Attendre l'état initial

if not initial_state:
    print("❌ Aucune réception d'état initial!")
    client.loop_stop()
    client.disconnect()
    exit(1)

print("\n📝 Envoi des commandes:\n")

# Test chaque relai avec ON/OFF
for relay_num in range(8):
    print(f"[Relai {relay_num}]")
    
    # ON
    print(f"  📤 Envoi: {relay_num}:on")
    client.publish("home/esp32/relay/cmd", f"{relay_num}:on", qos=1)
    time.sleep(1.5)
    
    if relay_history:
        current = relay_history[-1]['relays'].get(f'relay_{relay_num}')
        print(f"     ✅ Reçu: relay_{relay_num} = {current}")
    
    # OFF
    print(f"  📤 Envoi: {relay_num}:off")
    client.publish("home/esp32/relay/cmd", f"{relay_num}:off", qos=1)
    time.sleep(1.5)
    
    if relay_history:
        current = relay_history[-1]['relays'].get(f'relay_{relay_num}')
        print(f"     ✅ Reçu: relay_{relay_num} = {current}")
    
    print()

print("="*60)
print("RÉSUMÉ:")
print("="*60)

print("\n📊 État initial (au démarrage):")
for i in range(8):
    relay = f'relay_{i}'
    state = initial_state.get(relay, 0)
    expected = 0
    status = "✅" if state == expected else "❌"
    print(f"   {status} relay_{i}: {state} (attendu: {expected})")

print(f"\n📈 Nombre de changements détectés: {len(relay_history)}")

print("\n" + "="*60)

client.loop_stop()
client.disconnect()
