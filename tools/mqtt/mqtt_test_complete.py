#!/usr/bin/env python3
"""
Test complet du système MQTT: écoute tous les messages
puis envoie les commandes et montre les changements
"""

import paho.mqtt.client as mqtt
import sys
import time
import threading
from datetime import datetime

BROKER = "192.168.1.200"
PORT = 1883
USERNAME = "pascal"
PASSWORD = "123456"

# État suivi
relay_states = {}
command_count = 0
response_count = 0

def on_connect(client, userdata, flags, reason_code):
    print(f"\n[{datetime.now().strftime('%H:%M:%S')}] 🌐 Connecté au broker (code: {reason_code})")
    client.subscribe("#")
    print("[*] Écoute tous les topics...")

def on_message(client, userdata, msg):
    global relay_states, response_count, command_count
    
    topic = msg.topic
    payload = msg.payload.decode()
    
    if "relay/cmd" in topic:
        print(f"\n📤 COMMANDE ENVOYÉE: {payload}")
    
    elif "relay/status" in topic:
        try:
            import json
            status = json.loads(payload)
            # Comparer avec l'état précédent
            changed = False
            for relay, state in status.items():
                if relay not in relay_states or relay_states[relay] != state:
                    changed = True
                relay_states[relay] = state
            
            if changed:
                response_count += 1
                print(f"✅ RÉPONSE REÇUE (#{response_count})")
                for relay, state in status.items():
                    print(f"   {relay}: {'🟢 ON' if state else '⚫ OFF'}")
        except:
            pass

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

try:
    print(f"[{datetime.now().strftime('%H:%M:%S')}] Connexion à {BROKER}:{PORT}...")
    client.username_pw_set(USERNAME, PASSWORD)
    client.connect(BROKER, PORT, keepalive=60)
    client.loop_start()
    
    time.sleep(2)  # Attendre la connexion
    
    print("\n" + "="*60)
    print("TEST COMPLET MQTT - COMMANDS & RESPONSES")
    print("="*60)
    
    topic = "home/esp32/relay/cmd"
    
    # Test des commandes progressives
    test_commands = [
        ("Allumer relai 0", "0:on"),
        ("Attendre 2s...", None),
        ("Allumer relai 1", "1:on"),
        ("Attendre 2s...", None),
        ("Éteindre relai 0", "0:off"),
        ("Attendre 2s...", None),
        ("Toggle relai 2", "2:toggle"),
        ("Attendre 2s...", None),
        ("Allumer tous", "ALL:on"),
        ("Attendre 2s...", None),
        ("Éteindre tous", "ALL:off"),
    ]
    
    for desc, cmd in test_commands:
        if cmd:
            command_count += 1
            print(f"\n[Commande #{command_count}] {desc}")
            info = client.publish(topic, cmd, qos=1)
            print(f"  Send: {cmd} (rc={info.rc})")
        else:
            print(f"\n⏳ {desc}")
            time.sleep(2)
    
    print(f"\n" + "="*60)
    print("RÉSUMÉ:")
    print(f"  📤 Commandes envoyées: {command_count}")
    print(f"  ✅ Changements détectés: {response_count}")
    print(f"  État final des relais:")
    for relay, state in sorted(relay_states.items()):
        print(f"    {relay}: {'🟢 ON' if state else '⚫ OFF'}")
    print("="*60)
    
    time.sleep(2)
    client.loop_stop()
    client.disconnect()
    
except Exception as e:
    print(f"✗ Erreur: {e}")
