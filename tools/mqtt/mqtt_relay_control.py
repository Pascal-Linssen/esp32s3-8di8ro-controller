#!/usr/bin/env python3
"""
MQTT Remote Control for ESP32 8DI/8RO
Allows sending commands to control relays from web interface or CLI
"""

import paho.mqtt.client as mqtt
import os
import sys
import time

# Configuration
BROKER_HOST = os.getenv("MQTT_HOST", "192.168.1.200")
BROKER_PORT = int(os.getenv("MQTT_PORT", "1883"))
BROKER_USER = os.getenv("MQTT_USERNAME", "")
BROKER_PASSWORD = os.getenv("MQTT_PASSWORD", "")
CLIENT_ID = "web-remote-control"

RELAY_CMD_TOPIC = "waveshare/relay/cmd"
RELAY_STATUS_TOPIC = "waveshare/relay/status"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✅ Connecté au broker MQTT")
        client.subscribe(RELAY_STATUS_TOPIC)
    else:
        print(f"❌ Erreur connexion: {rc}")

def on_message(client, userdata, msg):
    """Display relay status updates"""
    try:
        import json
        data = json.loads(msg.payload.decode())
        status_str = " ".join([f"R{i}:{'ON' if data[f'relay_{i}'] else 'OFF'}" for i in range(8)])
        print(f"📊 État relais: {status_str}")
    except:
        pass

def send_command(command):
    """Send relay command via MQTT"""
    try:
        client = mqtt.Client(client_id=CLIENT_ID)
        if BROKER_USER:
            client.username_pw_set(BROKER_USER, BROKER_PASSWORD)
        client.on_connect = on_connect
        client.on_message = on_message
        
        print(f"🔌 Connexion au broker {BROKER_HOST}:{BROKER_PORT}...")
        client.connect(BROKER_HOST, BROKER_PORT, 60)
        
        client.loop_start()
        time.sleep(1)
        
        print(f"📤 Envoi commande: {command}")
        client.publish(RELAY_CMD_TOPIC, command, qos=1)
        
        time.sleep(2)
        client.loop_stop()
        print("✓ Commande envoyée")
        
    except Exception as e:
        print(f"❌ Erreur: {e}")

def interactive_menu():
    """Interactive command menu"""
    print("""
╔════════════════════════════════════════╗
║   MQTT Remote Relay Control            ║
║   ESP32-S3 8DI/8RO Controller          ║
╚════════════════════════════════════════╝

Commandes disponibles:
  0-7        : Basculer relais 0-7 (ex: "3" pour relais 3)
  ON:N       : Allumer relais N (ex: "ON:2")
  OFF:N      : Éteindre relais N (ex: "OFF:5")
  ALL:on     : Tous les relais ON
  ALL:off    : Tous les relais OFF
  ALL:toggle : Basculer tous les relais
  status    : Afficher état (test_mqtt_fixed.py)
  q/exit    : Quitter

""")
    
    while True:
        try:
            cmd = input("➤ Commande: ").strip()
            
            if cmd.lower() in ['q', 'exit', 'quit']:
                print("👋 Au revoir!")
                break
            
            if cmd.lower() == 'status':
                print("Pour voir l'état: python test_mqtt_fixed.py")
                continue
            
            # Parse single relay toggle (0-7)
            if cmd.isdigit() and 0 <= int(cmd) < 8:
                cmd = f"{cmd}:toggle"
            
            # Validate format
            valid_commands = ['ALL:on', 'ALL:off', 'ALL:toggle']
            valid_commands += [f"{i}:toggle" for i in range(8)]
            valid_commands += [f"ON:{i}" for i in range(8)]
            valid_commands += [f"OFF:{i}" for i in range(8)]
            
            if cmd not in valid_commands:
                print(f"❌ Commande invalide: {cmd}")
                continue
            
            send_command(cmd)
            
        except KeyboardInterrupt:
            print("\n👋 Arrêt...")
            break
        except Exception as e:
            print(f"❌ Erreur: {e}")

if __name__ == '__main__':
    if len(sys.argv) > 1:
        # Command line mode
        cmd = sys.argv[1]
        send_command(cmd)
    else:
        # Interactive mode
        interactive_menu()
