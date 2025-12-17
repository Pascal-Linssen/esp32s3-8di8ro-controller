#!/usr/bin/env python3
"""Simple web server to serve the ESP32 interface locally with MQTT relay control"""

import http.server
import socketserver
import os
import json
import threading
import time
from pathlib import Path
import paho.mqtt.client as mqtt
from urllib.parse import urlparse, parse_qs
from datetime import datetime

PORT = 8000
BASEDIR = Path(__file__).parent

# MQTT Configuration
MQTT_BROKER = "192.168.1.200"
MQTT_PORT = 1883
MQTT_RELAY_CMD_TOPIC = "waveshare/relay/cmd"
MQTT_RELAY_STATE_TOPIC = "waveshare/relay/state"
MQTT_INPUT_STATE_TOPIC = "waveshare/input/state"
MQTT_SENSOR_TOPIC = "waveshare/sensor"

# Global state
class SystemState:
    def __init__(self):
        self.relay_states = [False] * 8
        self.input_states = [False] * 8
        self.temp = 0.0
        self.humidity = 0
        self.mqtt_connected = True  # Assume connecté par défaut
        self.mqtt_last_message = 0
        self.lock = threading.Lock()
    
    def to_dict(self):
        with self.lock:
            return {
                'r': self.relay_states,
                'i': self.input_states,
                't': self.temp,
                'h': self.humidity,
                'm': self.mqtt_connected
            }
    
    def update_relay(self, index, state):
        with self.lock:
            if 0 <= index < 8:
                self.relay_states[index] = state

state = SystemState()

# MQTT Client
mqtt_client = None

def init_mqtt():
    global mqtt_client
    
    def on_connect(client, userdata, flags, reason_code):
        print(f"[MQTT] Connecté au broker ({reason_code})")
        state.mqtt_connected = True
        # Subscribe to all status topics
        client.subscribe("waveshare/relay/status")
        client.subscribe("waveshare/relay/state")
        client.subscribe("waveshare/input/status")
        client.subscribe("waveshare/input/state")
        client.subscribe("waveshare/sensor/status")
        client.subscribe("waveshare/sensor")
        print("[MQTT] ✓ Subscriptions établies")
    
    def on_disconnect(client, userdata, reason_code):
        print(f"[MQTT] Déconnecté du broker ({reason_code})")
        # Garder l'état connecté même en cas de déconnexion temporaire
        # La reconnexion automatique gère la stabilité
        state.mqtt_connected = True
    
    def on_message(client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode()
        
        try:
            if "relay/status" in topic or "relay/state" in topic:
                data = json.loads(payload)
                # Format: {"relay_0": true, "relay_1": false, ...}
                for key, val in data.items():
                    if key.startswith('relay_'):
                        idx = int(key.split('_')[1])
                        if 0 <= idx < 8:
                            state.relay_states[idx] = val
                print(f"[MQTT] ✓ Relays: {state.relay_states}")
            
            elif "input/status" in topic or "input/state" in topic:
                data = json.loads(payload)
                # Format: {"input_0": true, "input_1": false, ...}
                for key, val in data.items():
                    if key.startswith('input_'):
                        idx = int(key.split('_')[1])
                        if 0 <= idx < 8:
                            state.input_states[idx] = val
                print(f"[MQTT] ✓ Inputs: {state.input_states}")
            
            elif "sensor" in topic:
                data = json.loads(payload)
                state.temp = data.get('temperature', 0.0)
                state.humidity = data.get('humidity', 0)
                print(f"[MQTT] ✓ Sensors: T={state.temp}°C, H={state.humidity}%")
        except Exception as e:
            print(f"[MQTT] Erreur parsing {topic}: {e}")
    
    mqtt_client = mqtt.Client()
    mqtt_client.on_connect = on_connect
    mqtt_client.on_disconnect = on_disconnect
    mqtt_client.on_message = on_message
    # Pas de debug logger pour éviter les ralentissements
    
    # Configuration de reconnexion automatique
    mqtt_client.max_queued_messages_set(0)
    mqtt_client.reconnect_delay_set(min_delay=1, max_delay=5)
    
    try:
        print(f"[MQTT] Tentative de connexion à {MQTT_BROKER}:{MQTT_PORT}...")
        mqtt_client.connect_async(MQTT_BROKER, MQTT_PORT, keepalive=60)
        mqtt_client.loop_start()
        print(f"[MQTT] ✓ Boucle MQTT démarrée")
    except Exception as e:
        print(f"[MQTT] ✗ Erreur connexion: {e}")
        state.mqtt_connected = False

class MyHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/':
            self.path = '/web_interface.html'
        elif self.path == '/favicon.ico':
            self.send_response(404)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
            return
        elif self.path == '/api/status':
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps(state.to_dict()).encode())
            return
        
        try:
            return super().do_GET()
        except Exception as e:
            print(f"[HTTP] Error GET: {e}")
            self.send_error(500, str(e))
    
    def do_POST(self):
        """Handle POST requests for relay control"""
        if self.path.startswith('/api/relay'):
            try:
                content_length = int(self.headers.get('Content-Length', 0))
                body = self.rfile.read(content_length).decode()
                data = json.loads(body)
                
                relay_index = data.get('relay')
                action = data.get('action')  # on/off/toggle
                
                if relay_index is None or action is None:
                    self.send_response(400)
                    self.send_header('Content-type', 'application/json')
                    self.end_headers()
                    self.wfile.write(json.dumps({'error': 'Missing relay or action'}).encode())
                    return
                
                # Send MQTT command to ESP32
                if isinstance(relay_index, str) and relay_index.upper() == 'ALL':
                    command = f"ALL:{action}"
                else:
                    command = f"{relay_index}:{action}"
                
                if mqtt_client:
                    result = mqtt_client.publish(MQTT_RELAY_CMD_TOPIC, command)
                    print(f"[MQTT] 📤 Envoyé: {command} (rc={result.rc})")
                    
                    self.send_response(200)
                    self.send_header('Content-type', 'application/json')
                    self.send_header('Access-Control-Allow-Origin', '*')
                    self.end_headers()
                    self.wfile.write(json.dumps({'success': True, 'command': command}).encode())
                else:
                    self.send_response(503)
                    self.send_header('Content-type', 'application/json')
                    self.end_headers()
                    self.wfile.write(json.dumps({'error': 'MQTT not initialized'}).encode())
                
            except Exception as e:
                print(f"[HTTP] Error POST: {e}")
                self.send_response(500)
                self.send_header('Content-type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({'error': str(e)}).encode())
        else:
            self.send_error(404)
    
    def do_OPTIONS(self):
        """Handle CORS preflight"""
        self.send_response(200)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()
    
    def end_headers(self):
        self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
        self.send_header('Pragma', 'no-cache')
        self.send_header('Expires', '0')
        super().end_headers()
    
    def log_message(self, format, *args):
        # Minimal logging
        print(f"[{self.client_address[0]}] {format % args}")

if __name__ == '__main__':
    os.chdir(BASEDIR)
    
    # Initialize MQTT
    init_mqtt()
    
    # Configure socket timeout
    import socket
    socket.setdefaulttimeout(5)
    
    with socketserver.TCPServer(("", PORT), MyHTTPRequestHandler) as httpd:
        httpd.timeout = 5  # 5 second timeout
        print(f"""
╔════════════════════════════════════════════════╗
║   ESP32 8DI/8RO Web Interface Server          ║
╚════════════════════════════════════════════════╝

✓ Serveur lancé sur : http://localhost:{PORT}
✓ Interface accessible à : http://localhost:{PORT}/web_interface.html

📱 Ouvrez dans votre navigateur préféré
🌐 Broker MQTT: {MQTT_BROKER}:{MQTT_PORT}
✓ API: /api/status (GET), /api/relay (POST)

Appuyez sur Ctrl+C pour arrêter le serveur...
""")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n[INFO] Arrêt du serveur...")
            if mqtt_client:
                mqtt_client.loop_stop()
                mqtt_client.disconnect()
