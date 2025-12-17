#!/usr/bin/env python3
"""
Test tous les 8 relais
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

relay_states = {}

def on_connect(client, userdata, flags, reason_code):
    if reason_code == 0:
        print("✅ Connecté au broker")
        client.subscribe("#")

def on_message(client, userdata, msg):
    global relay_states
    
    if "relay/status" in msg.topic:
        try:
            data = json.loads(msg.payload.decode())
            relay_states = data
        except:
            pass

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
if USERNAME:
    client.username_pw_set(USERNAME, PASSWORD)
client.connect(BROKER, PORT, keepalive=60)
client.loop_start()

print("\n" + "="*70)
print("TEST TOUS LES 8 RELAIS")
print("="*70)

time.sleep(2)

# Tester chaque relai
for relay_num in range(8):
    print(f"\n[Relai {relay_num}]")
    
    # Allumer
    print(f"  📤 ON")
    client.publish("home/esp32/relay/cmd", f"{relay_num}:on", qos=1)
    time.sleep(1)
    
    if relay_states:
        state = relay_states.get(f'relay_{relay_num}')
        print(f"     État: {state} {'✅' if state == 1 else '❌'}")
    
    # Éteindre
    print(f"  📤 OFF")
    client.publish("home/esp32/relay/cmd", f"{relay_num}:off", qos=1)
    time.sleep(1)
    
    if relay_states:
        state = relay_states.get(f'relay_{relay_num}')
        print(f"     État: {state} {'✅' if state == 0 else '❌'}")

print(f"\n" + "="*70)
print("RÉSUMÉ FINAL:")
print("="*70)
if relay_states:
    print(f"État actuel de tous les relais:")
    for i in range(8):
        state = relay_states.get(f'relay_{i}')
        print(f"  Relai {i}: {'🟢 ON' if state else '⚫ OFF'}")

print("\n" + "="*70)

client.loop_stop()
client.disconnect()
