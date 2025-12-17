#!/usr/bin/env python3
"""
Mini-broker MQTT local pour tester l'ESP32
Lance un broker léger qui affiche tous les messages
"""

import socket
import threading
import time
from datetime import datetime

class SimpleMQTTBroker:
    def __init__(self, host='0.0.0.0', port=1883):
        self.host = host
        self.port = port
        self.server = None
        self.running = False
        self.clients = []
        self.messages = {}
        
    def start(self):
        """Démarrer le broker"""
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server.bind((self.host, self.port))
        self.server.listen(5)
        self.running = True
        
        print(f"\n{'='*60}")
        print(f"🚀 Mini-broker MQTT démarré sur {self.host}:{self.port}")
        print(f"{'='*60}\n")
        
        threading.Thread(target=self._accept_connections, daemon=True).start()
        
    def _accept_connections(self):
        """Accepter les connexions clients"""
        while self.running:
            try:
                client, addr = self.server.accept()
                print(f"✓ Connexion: {addr[0]}:{addr[1]}")
                threading.Thread(
                    target=self._handle_client,
                    args=(client, addr),
                    daemon=True
                ).start()
            except Exception as e:
                if self.running:
                    print(f"✗ Erreur connexion: {e}")
    
    def _handle_client(self, client, addr):
        """Gérer un client MQTT"""
        try:
            buffer = b''
            while self.running:
                data = client.recv(1024)
                if not data:
                    break
                
                buffer += data
                
                # Traiter les messages MQTT (simplifié)
                while len(buffer) > 0:
                    # Chercher les patterns de publication/souscription
                    if b'waveshare/' in buffer:
                        self._process_mqtt_data(buffer, client)
                    buffer = buffer[len(buffer):]
                    break
                    
        except Exception as e:
            print(f"✗ Erreur client: {e}")
        finally:
            try:
                client.close()
            except:
                pass
            print(f"✗ Déconnecté: {addr[0]}:{addr[1]}")
    
    def _process_mqtt_data(self, data, client):
        """Traiter les données MQTT"""
        try:
            text = data.decode('utf-8', errors='ignore')
            
            # Afficher les topics
            if 'waveshare/' in text:
                print(f"📨 {datetime.now().strftime('%H:%M:%S')} | Reçu: {text[:100]}")
                
        except:
            pass
    
    def stop(self):
        """Arrêter le broker"""
        self.running = False
        if self.server:
            self.server.close()
        print(f"\n{'='*60}")
        print("⏹️  Broker arrêté")
        print(f"{'='*60}\n")


def main():
    broker = SimpleMQTTBroker(host='0.0.0.0', port=1883)
    broker.start()
    
    print("Appuyez sur Ctrl+C pour arrêter...\n")
    
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        broker.stop()


if __name__ == "__main__":
    main()
