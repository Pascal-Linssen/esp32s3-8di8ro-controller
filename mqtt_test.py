#!/usr/bin/env python3
"""
Script de test MQTT pour ESP32-S3-ETH-8DI-8RO
Teste la communication avec le broker Mosquitto @ 192.168.1.200:1883
"""

import paho.mqtt.client as mqtt
import json
import time
import sys

# Configuration
BROKER_HOST = "192.168.1.200"
BROKER_PORT = 1883
MQTT_USER = "pascal"
MQTT_PASSWORD = "123456"
CLIENT_ID = "mqtt-tester"

# Topics
TOPIC_RELAY_CMD = "home/esp32/relay/cmd"
TOPIC_RELAY_STATUS = "home/esp32/relay/status"
TOPIC_INPUT_STATUS = "home/esp32/input/status"
TOPIC_SENSOR_STATUS = "home/esp32/sensor/status"
TOPIC_SYSTEM_STATUS = "home/esp32/system/status"

# Variables globales
received_messages = {}
connected = False


def on_connect(client, userdata, flags, rc):
    """Callback de connexion"""
    global connected
    if rc == 0:
        print("✓ Connecté au broker MQTT")
        connected = True
        
        # Souscrire aux topics de statut
        client.subscribe(TOPIC_RELAY_STATUS)
        client.subscribe(TOPIC_INPUT_STATUS)
        client.subscribe(TOPIC_SENSOR_STATUS)
        client.subscribe(TOPIC_SYSTEM_STATUS)
        print("✓ Souscrits aux topics de statut")
    else:
        print(f"✗ Erreur de connexion: code {rc}")
        connected = False


def on_disconnect(client, userdata, rc):
    """Callback de déconnexion"""
    global connected
    connected = False
    if rc != 0:
        print(f"✗ Déconnexion inattendue: code {rc}")


def on_message(client, userdata, msg):
    """Callback de message reçu"""
    topic = msg.topic
    payload = msg.payload.decode('utf-8')
    
    received_messages[topic] = payload
    
    print(f"\n📨 Message reçu: {topic}")
    
    # Parser le JSON si possible
    try:
        data = json.loads(payload)
        print(f"   Données: {json.dumps(data, indent=2)}")
    except:
        print(f"   Données brutes: {payload}")


def publish_command(client, relay_id, state):
    """Publier une commande de relais"""
    payload = f"{relay_id}:{state}"
    print(f"\n📤 Publication: {TOPIC_RELAY_CMD} = {payload}")
    client.publish(TOPIC_RELAY_CMD, payload)


def test_individual_relays(client):
    """Tester chaque relais individuellement"""
    print("\n" + "="*60)
    print("TEST 1: Contrôle des relais individuels")
    print("="*60)
    
    for relay in range(8):
        print(f"\n>>> Allumage relais {relay}...")
        publish_command(client, relay, "on")
        time.sleep(1)
        
        if TOPIC_RELAY_STATUS in received_messages:
            print(f"État reçu: {received_messages[TOPIC_RELAY_STATUS]}")
    
    print(f"\n>>> Extinction de tous les relais...")
    for relay in range(8):
        publish_command(client, relay, "off")
        time.sleep(0.5)


def test_all_relays(client):
    """Tester tous les relais à la fois"""
    print("\n" + "="*60)
    print("TEST 2: Contrôle de tous les relais")
    print("="*60)
    
    print("\n>>> Allumage de TOUS les relais...")
    publish_command(client, "ALL", "on")
    time.sleep(2)
    
    if TOPIC_RELAY_STATUS in received_messages:
        print(f"État reçu: {received_messages[TOPIC_RELAY_STATUS]}")
    
    print("\n>>> Extinction de TOUS les relais...")
    publish_command(client, "ALL", "off")
    time.sleep(2)
    
    if TOPIC_RELAY_STATUS in received_messages:
        print(f"État reçu: {received_messages[TOPIC_RELAY_STATUS]}")


def test_status_monitoring(client):
    """Surveiller les statuts du système"""
    print("\n" + "="*60)
    print("TEST 3: Surveillance des statuts (15 secondes)")
    print("="*60)
    
    start_time = time.time()
    last_relay = ""
    
    while time.time() - start_time < 15:
        # Afficher les changements
        if TOPIC_RELAY_STATUS in received_messages and received_messages[TOPIC_RELAY_STATUS] != last_relay:
            last_relay = received_messages[TOPIC_RELAY_STATUS]
            print(f"\n📊 Relais: {last_relay}")
        
        if TOPIC_INPUT_STATUS in received_messages:
            print(f"📊 Entrées: {received_messages[TOPIC_INPUT_STATUS]}", end='\r')
        
        if TOPIC_SENSOR_STATUS in received_messages:
            try:
                sensors = json.loads(received_messages[TOPIC_SENSOR_STATUS])
                print(f"📊 Temp: {sensors.get('temperature', '?')}°C, Hum: {sensors.get('humidity', '?')}%", end='\r')
            except:
                pass
        
        client.loop()
        time.sleep(0.1)
    
    print("\n")


def test_rapid_commands(client):
    """Test de commandes rapides"""
    print("\n" + "="*60)
    print("TEST 4: Commandes rapides (alternance ON/OFF)")
    print("="*60)
    
    relay = 0
    for i in range(5):
        state = "on" if i % 2 == 0 else "off"
        print(f"\n>>> Itération {i+1}: Relais {relay} → {state}")
        publish_command(client, relay, state)
        time.sleep(0.8)


def display_all_status(client):
    """Afficher tous les statuts reçus"""
    print("\n" + "="*60)
    print("STATUTS ACTUELS DU SYSTÈME")
    print("="*60)
    
    # Attendre un peu pour recevoir tous les messages
    print("Attente de réception des statuts...")
    for _ in range(10):
        client.loop()
        time.sleep(0.1)
    
    if TOPIC_RELAY_STATUS in received_messages:
        try:
            relays = json.loads(received_messages[TOPIC_RELAY_STATUS])
            print(f"\n✓ Relais: {relays}")
        except:
            print(f"\n✓ Relais: {received_messages[TOPIC_RELAY_STATUS]}")
    
    if TOPIC_INPUT_STATUS in received_messages:
        try:
            inputs = json.loads(received_messages[TOPIC_INPUT_STATUS])
            print(f"✓ Entrées: {inputs}")
        except:
            print(f"✓ Entrées: {received_messages[TOPIC_INPUT_STATUS]}")
    
    if TOPIC_SENSOR_STATUS in received_messages:
        try:
            sensors = json.loads(received_messages[TOPIC_SENSOR_STATUS])
            print(f"✓ Capteurs: Temp={sensors.get('temperature')}°C, Hum={sensors.get('humidity')}%")
        except:
            print(f"✓ Capteurs: {received_messages[TOPIC_SENSOR_STATUS]}")
    
    if TOPIC_SYSTEM_STATUS in received_messages:
        try:
            system = json.loads(received_messages[TOPIC_SYSTEM_STATUS])
            print(f"✓ Système: IP={system.get('ip')}, Uptime={system.get('uptime')}s")
        except:
            print(f"✓ Système: {received_messages[TOPIC_SYSTEM_STATUS]}")


def main():
    """Fonction principale"""
    print("\n" + "="*60)
    print("TEST MQTT - ESP32-S3-ETH-8DI-8RO")
    print("="*60)
    print(f"Broker: {BROKER_HOST}:{BROKER_PORT}")
    print(f"User: {MQTT_USER}")
    print(f"Client ID: {CLIENT_ID}")
    
    # Créer le client MQTT
    client = mqtt.Client(client_id=CLIENT_ID)
    client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    
    # Connexion
    print(f"\n📡 Connexion au broker...")
    try:
        client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)
        client.loop_start()
    except Exception as e:
        print(f"✗ Erreur de connexion: {e}")
        return False
    
    # Attendre la connexion
    timeout = time.time() + 5
    while not connected and time.time() < timeout:
        time.sleep(0.1)
    
    if not connected:
        print("✗ Impossible de se connecter au broker dans le délai")
        return False
    
    try:
        # Afficher les statuts initiaux
        display_all_status(client)
        
        # Exécuter les tests
        input("\n>>> Appuyez sur Entrée pour commencer les tests de relais...")
        
        test_individual_relays(client)
        time.sleep(2)
        
        test_all_relays(client)
        time.sleep(2)
        
        test_rapid_commands(client)
        time.sleep(2)
        
        test_status_monitoring(client)
        
        # Afficher les résultats finaux
        display_all_status(client)
        
        print("\n" + "="*60)
        print("✓ TESTS COMPLÉTÉS")
        print("="*60)
        
    except KeyboardInterrupt:
        print("\n\n⚠️  Tests interrompus par l'utilisateur")
    except Exception as e:
        print(f"\n✗ Erreur durant les tests: {e}")
        import traceback
        traceback.print_exc()
    finally:
        client.loop_stop()
        client.disconnect()
    
    return True


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
