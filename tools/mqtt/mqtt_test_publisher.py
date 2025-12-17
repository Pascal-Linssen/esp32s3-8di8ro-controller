#!/usr/bin/env python3
"""
Publie des commandes de test au broker MQTT
"""

import paho.mqtt.client as mqtt
import sys
import time
from datetime import datetime
import os

BROKER = os.getenv("MQTT_HOST", "192.168.1.200")
PORT = int(os.getenv("MQTT_PORT", "1883"))
USERNAME = os.getenv("MQTT_USERNAME", "")
PASSWORD = os.getenv("MQTT_PASSWORD", "")

def on_connect(client, userdata, flags, reason_code):
    print(f"[{datetime.now().strftime('%H:%M:%S')}] ✓ Connecté au broker (code: {reason_code})")

def on_publish(client, userdata, mid):
    print(f"[{datetime.now().strftime('%H:%M:%S')}] ✓ Message publié (id: {mid})")

def on_disconnect(client, userdata, reason_code):
    if reason_code != 0:
        print(f"✗ Déconnecté (code: {reason_code})")

client = mqtt.Client()
client.on_connect = on_connect
client.on_publish = on_publish
client.on_disconnect = on_disconnect

try:
    print(f"[{datetime.now().strftime('%H:%M:%S')}] Connexion à {BROKER}:{PORT}...")
    if USERNAME:
        client.username_pw_set(USERNAME, PASSWORD)
    client.connect(BROKER, PORT, keepalive=60)
    client.loop_start()
    
    time.sleep(1)  # Attendre la connexion
    
    # Tester avec différentes commandes
    commands = [
        ("0:on", "Allumer relai 0"),
        ("0:off", "Éteindre relai 0"),
        ("1:on", "Allumer relai 1"),
        ("1:off", "Éteindre relai 1"),
        ("2:toggle", "Toggle relai 2"),
    ]
    
    topic = "home/esp32/relay/cmd"
    
    for payload, desc in commands:
        print(f"\n[{datetime.now().strftime('%H:%M:%S')}] 📤 Envoi: {desc}")
        print(f"   Topic: {topic}")
        print(f"   Payload: {payload}")
        
        info = client.publish(topic, payload, qos=1)
        print(f"   Return code: {info.rc} (0=succès)")
        
        time.sleep(2)  # Attendre 2s entre les commandes
    
    print(f"\n[{datetime.now().strftime('%H:%M:%S')}] ✓ Test terminalé")
    client.loop_stop()
    client.disconnect()
    
except Exception as e:
    print(f"✗ Erreur: {e}")
