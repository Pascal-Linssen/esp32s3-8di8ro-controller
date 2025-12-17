#!/usr/bin/env python3
"""
Test avec capture des logs serials
"""

import paho.mqtt.client as mqtt
import time
import json
import subprocess
import threading
from datetime import datetime

BROKER = "192.168.1.200"
PORT = 1883
USERNAME = "pascal"
PASSWORD = "123456"

relay_history = []
serial_logs = []

def on_connect(client, userdata, flags, reason_code):
    if reason_code == 0:
        print("✅ Connecté au broker MQTT")
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
            if len(relay_history) > 1:
                prev = relay_history[-2]['relays']
                curr = relay_history[-1]['relays']
                for i in range(8):
                    if prev.get(f'relay_{i}') != curr.get(f'relay_{i}'):
                        print(f"\n🔄 [{datetime.now().strftime('%H:%M:%S')}] relay_{i}: {prev.get(f'relay_{i}')} → {curr.get(f'relay_{i}')}")
        except:
            pass

# Configuration MQTT
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.username_pw_set(USERNAME, PASSWORD)
client.connect(BROKER, PORT, keepalive=60)
client.loop_start()

print("\n" + "="*60)
print("TEST AVEC LOGS SERIALS")
print("="*60)

time.sleep(2)

# Test simple: relai 0, 1, 2
tests = [
    ("0:on", "Allumer relai 0"),
    ("0:off", "Éteindre relai 0"),
    ("1:on", "Allumer relai 1"),
    ("1:off", "Éteindre relai 1"),
    ("2:on", "Allumer relai 2"),
    ("2:off", "Éteindre relai 2"),
]

for cmd, desc in tests:
    print(f"\n📤 {desc}")
    print(f"   Envoi: {cmd}")
    client.publish("home/esp32/relay/cmd", cmd, qos=1)
    
    print(f"   ⏳ Attente 2s...")
    time.sleep(2)

print(f"\n" + "="*60)
print("RÉSUMÉ FINAL:")
print("="*60)

if relay_history:
    print(f"\nChangements détectés: {len(relay_history)}")
    for i, entry in enumerate(relay_history[-6:]):
        print(f"[{entry['time']}] {entry['relays']}")

client.loop_stop()
client.disconnect()
