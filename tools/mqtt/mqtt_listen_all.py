#!/usr/bin/env python3
"""
Écouter TOUS les messages MQTT de l'ESP32
"""

import paho.mqtt.client as mqtt
from datetime import datetime
import os

BROKER = os.getenv("MQTT_HOST", "192.168.1.200")
PORT = int(os.getenv("MQTT_PORT", "1883"))
USERNAME = os.getenv("MQTT_USERNAME", "")
PASSWORD = os.getenv("MQTT_PASSWORD", "")

count = 0

def on_connect(client, userdata, flags, reason_code):
    print(f"[{datetime.now().strftime('%H:%M:%S')}] ✅ Connecté au broker")
    client.subscribe("#")

def on_message(client, userdata, msg):
    global count
    count += 1
    print(f"[{datetime.now().strftime('%H:%M:%S')}] #{count} {msg.topic}: {msg.payload.decode()[:100]}")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
if USERNAME:
    client.username_pw_set(USERNAME, PASSWORD)
client.connect(BROKER, PORT, keepalive=60)

print("\n" + "="*70)
print("ÉCOUTE DE TOUS LES MESSAGES MQTT")
print("="*70 + "\n")

try:
    client.loop_forever()
except KeyboardInterrupt:
    print(f"\n[{datetime.now().strftime('%H:%M:%S')}] ⏹ Arrêt... ({count} messages reçus)")
    client.disconnect()
