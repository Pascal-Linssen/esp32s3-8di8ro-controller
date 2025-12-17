#!/usr/bin/env python3
"""Test MQTT publish to ESP32"""

import paho.mqtt.client as mqtt
import time
import sys

# Configuration MQTT (MÊME que broker)
BROKER = "192.168.1.200"
PORT = 1883
USER = "pascal"
PASSWORD = "123456"
TOPIC_CMD = "home/esp32/relay/cmd"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✓ Connecté au broker: {BROKER}:{PORT}")
    else:
        print(f"✗ Erreur de connexion: code {rc}")

def on_disconnect(client, userdata, rc):
    if rc != 0:
        print(f"✗ Déconnexion inattendue: code {rc}")

# Créer le client
client = mqtt.Client()
client.on_connect = on_connect
client.on_disconnect = on_disconnect

# Connecter
print(f"Connexion à {BROKER}:{PORT}...")
client.username_pw_set(USER, PASSWORD)
client.connect(BROKER, PORT, keepalive=60)
client.loop_start()

# Attendre la connexion
time.sleep(2)

# Envoyer des commandes
commands = ["0:on", "0:off", "1:on", "1:off", "ALL:on", "ALL:off"]

for cmd in commands:
    print(f"\n📤 Envoi: {TOPIC_CMD} = {cmd}")
    result = client.publish(TOPIC_CMD, cmd, qos=1)
    
    if result.rc == mqtt.MQTT_ERR_SUCCESS:
        print(f"   ✓ Publié")
    else:
        print(f"   ✗ Erreur: {result.rc}")
    
    time.sleep(1)

print("\n✓ Tous les messages envoyés")
client.loop_stop()
client.disconnect()
