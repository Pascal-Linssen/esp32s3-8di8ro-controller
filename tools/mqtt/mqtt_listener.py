#!/usr/bin/env python3
"""Listen to all MQTT messages"""

import paho.mqtt.client as mqtt
import sys

BROKER = "192.168.1.200"
PORT = 1883

def on_connect(client, userdata, flags, reason_code):
    print(f"[MQTT] ✓ Connecté au broker ({reason_code})")
    client.subscribe("#")  # Subscribe to ALL topics
    print("[MQTT] Écoute de tous les topics...")

def on_message(client, userdata, msg):
    print(f"\n📨 Topic: {msg.topic}")
    print(f"   Payload: {msg.payload.decode()}")
    print(f"   QoS: {msg.qos}")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

try:
    print(f"Connexion à {BROKER}:{PORT}...")
    client.connect(BROKER, PORT, keepalive=60)
    client.loop_forever()
except KeyboardInterrupt:
    print("\n\nArrêt...")
    client.disconnect()
except Exception as e:
    print(f"✗ Erreur: {e}")
