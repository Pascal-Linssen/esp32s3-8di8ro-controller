#!/usr/bin/env python3
"""
Configuration MQTT manager pour ESP32-S3-ETH-8DI-8RO
Permet de modifier les paramètres MQTT via CLI sans recompiler
"""

import json
import os

CONFIG_FILE = "mqtt_config.json"

# Configuration par défaut
DEFAULT_CONFIG = {
    "broker_ip": "192.168.1.200",
    "broker_port": 1883,
    "username": "",
    "password": "",
    "topic_relay_cmd": "waveshare/relay/cmd",
    "topic_relay_status": "waveshare/relay/status",
    "topic_input_status": "waveshare/input/status",
    "topic_sensor_status": "waveshare/sensor/status",
    "topic_system_status": "waveshare/system/status"
}

def load_config():
    """Charger la config depuis le fichier"""
    if os.path.exists(CONFIG_FILE):
        with open(CONFIG_FILE, 'r') as f:
            return json.load(f)
    return DEFAULT_CONFIG.copy()

def save_config(config):
    """Sauvegarder la config dans le fichier"""
    with open(CONFIG_FILE, 'w') as f:
        json.dump(config, f, indent=2)
    print(f"✓ Configuration sauvegardée dans {CONFIG_FILE}")

def display_config(config):
    """Afficher la configuration actuelle"""
    print("\n" + "="*60)
    print("📡 CONFIGURATION MQTT ACTUELLE")
    print("="*60)
    print(f"\n🔗 Broker: {config['broker_ip']}:{config['broker_port']}")
    print(f"👤 Utilisateur: {config['username']}")
    print(f"🔐 Mot de passe: {'*' * len(config['password'])}")
    print(f"\n📨 Topics:")
    print(f"   Relais (CMD):    {config['topic_relay_cmd']}")
    print(f"   Relais (STATUS): {config['topic_relay_status']}")
    print(f"   Entrées:         {config['topic_input_status']}")
    print(f"   Capteurs:        {config['topic_sensor_status']}")
    print(f"   Système:         {config['topic_system_status']}")
    print()

def edit_interactive(config):
    """Édition interactive de la config"""
    print("\n" + "="*60)
    print("✏️  ÉDITION CONFIGURATION")
    print("="*60)
    
    while True:
        display_config(config)
        print("Choisissez une option:")
        print("1. Modifier adresse broker")
        print("2. Modifier port broker")
        print("3. Modifier utilisateur")
        print("4. Modifier mot de passe")
        print("5. Modifier topics")
        print("6. Réinitialiser aux défauts")
        print("0. Quitter")
        
        choice = input("\nVotre choix: ").strip()
        
        if choice == "1":
            ip = input(f"Nouvelle IP ({config['broker_ip']}): ").strip()
            if ip:
                config['broker_ip'] = ip
                print(f"✓ Broker IP: {ip}")
        
        elif choice == "2":
            port = input(f"Nouveau port ({config['broker_port']}): ").strip()
            if port and port.isdigit():
                config['broker_port'] = int(port)
                print(f"✓ Broker port: {port}")
        
        elif choice == "3":
            user = input(f"Nouvel utilisateur ({config['username']}): ").strip()
            if user:
                config['username'] = user
                print(f"✓ Utilisateur: {user}")
        
        elif choice == "4":
            pwd = input(f"Nouveau mot de passe: ").strip()
            if pwd:
                config['password'] = pwd
                print(f"✓ Mot de passe modifié")
        
        elif choice == "5":
            print("\nTopics disponibles:")
            topics_list = [
                ('topic_relay_cmd', 'Commandes relais'),
                ('topic_relay_status', 'Statut relais'),
                ('topic_input_status', 'Statut entrées'),
                ('topic_sensor_status', 'Statut capteurs'),
                ('topic_system_status', 'Statut système')
            ]
            for i, (key, desc) in enumerate(topics_list, 1):
                print(f"{i}. {desc}: {config[key]}")
            print("0. Retour")
            
            topic_choice = input("\nChoisissez un topic (1-5): ").strip()
            if topic_choice.isdigit() and 1 <= int(topic_choice) <= 5:
                key = topics_list[int(topic_choice)-1][0]
                new_topic = input(f"Nouveau topic ({config[key]}): ").strip()
                if new_topic:
                    config[key] = new_topic
                    print(f"✓ {topics_list[int(topic_choice)-1][1]}: {new_topic}")
        
        elif choice == "6":
            confirm = input("\nRéinitialiser aux défauts? (y/n): ").strip().lower()
            if confirm == 'y':
                config = DEFAULT_CONFIG.copy()
                print("✓ Configuration réinitialisée")
        
        elif choice == "0":
            confirm = input("\nSauvegarder les modifications? (y/n): ").strip().lower()
            if confirm == 'y':
                save_config(config)
            break
        
        print()
    
    return config

def main():
    print("\n" + "="*60)
    print("🔧 CONFIGURATION MQTT - ESP32-S3-ETH")
    print("="*60)
    
    # Charger la config
    config = load_config()
    
    # Menu principal
    while True:
        print("\nOptions:")
        print("1. Afficher configuration")
        print("2. Éditer configuration")
        print("3. Générer JSON pour SPIFFS")
        print("0. Quitter")
        
        choice = input("\nVotre choix: ").strip()
        
        if choice == "1":
            display_config(config)
        
        elif choice == "2":
            config = edit_interactive(config)
        
        elif choice == "3":
            json_str = json.dumps(config)
            print(f"\n✓ JSON (copier dans SPIFFS):\n{json_str}")
            
            # Sauvegarder aussi
            if input("\nSauvegarder dans fichier? (y/n): ").strip().lower() == 'y':
                save_config(config)
        
        elif choice == "0":
            print("\nAu revoir!")
            break

if __name__ == "__main__":
    main()
