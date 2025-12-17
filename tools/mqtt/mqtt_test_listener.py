#!/usr/bin/env python3
"""
Écoute tous les messages sur le broker MQTT
Pour tester si les commandes arrivent avant d'être envoyées à l'ESP32
"""

import paho.mqtt.client as mqtt
import sys
from datetime import datetime

import os

BROKER = os.getenv("MQTT_HOST", "192.168.1.200")
PORT = int(os.getenv("MQTT_PORT", "1883"))
USERNAME = os.getenv("MQTT_USERNAME", "")
PASSWORD = os.getenv("MQTT_PASSWORD", "")

# Compteurs
messages_received = 0
relay_commands = 0
relay_status = 0

def on_connect(client, userdata, flags, reason_code):
    print(f"[{datetime.now().strftime('%H:%M:%S')}] ✓ Connecté au broker (code: {reason_code})")
    client.subscribe("#")  # Subscribe to ALL topics
    print("[*] Écoute de tous les topics...")

def on_message(client, userdata, msg):
    global messages_received, relay_commands, relay_status
    messages_received += 1
    
    topic = msg.topic
    payload = msg.payload.decode()
    
    if "relay/cmd" in topic:
        relay_commands += 1
        print(f"\n🎯 COMMANDE RELAIS DÉTECTÉE!")
        print(f"   Topic: {topic}")
        print(f"   Payload: {payload}")
        print(f"   QoS: {msg.qos}")
    elif "relay/status" in topic:
        relay_status += 1
        print(f"\n📊 Statut relais")
        print(f"   Topic: {topic}")
        print(f"   Payload: {payload}")
    else:
        print(f"\n📨 Message:")
        print(f"   Topic: {topic}")
        print(f"   Payload: {payload}")
    
    print(f"   [Total: {messages_received} | Cmds: {relay_commands} | Status: {relay_status}]")

def on_disconnect(client, userdata, reason_code):
    if reason_code != 0:
        print(f"✗ Déconnecté (code: {reason_code})")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect

try:
    print(f"[{datetime.now().strftime('%H:%M:%S')}] Connexion à {BROKER}:{PORT}...")
    if USERNAME:
        client.username_pw_set(USERNAME, PASSWORD)
    client.connect(BROKER, PORT, keepalive=60)
    client.loop_forever()
except KeyboardInterrupt:
    print(f"\n\n[{datetime.now().strftime('%H:%M:%S')}] ⏹ Arrêt...")
    client.disconnect()
except Exception as e:
    print(f"✗ Erreur: {e}")
