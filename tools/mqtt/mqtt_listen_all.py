#!/usr/bin/env python3
"""
Écouter TOUS les messages MQTT de l'ESP32
"""

import paho.mqtt.client as mqtt
from datetime import datetime

BROKER = "192.168.1.200"
PORT = 1883
USERNAME = "pascal"
PASSWORD = "123456"

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
