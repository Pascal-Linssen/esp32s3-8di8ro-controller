#!/usr/bin/env python3
"""
MQTT Test Script for ESP32-S3 8DI/8RO Controller
Tests both relay commands and status feedback
"""

import paho.mqtt.client as mqtt
import json
import time
import sys
import os

# Configuration
BROKER_HOST = os.getenv("MQTT_HOST", "192.168.1.200")
BROKER_PORT = int(os.getenv("MQTT_PORT", "1883"))
BROKER_USER = os.getenv("MQTT_USERNAME", "")
BROKER_PASSWORD = os.getenv("MQTT_PASSWORD", "")
CLIENT_ID = "test-script"

# Topics
RELAY_CMD_TOPIC = "home/esp32/relay/cmd"
RELAY_STATUS_TOPIC = "home/esp32/relay/status"
INPUT_STATUS_TOPIC = "home/esp32/input/status"
SENSOR_STATUS_TOPIC = "home/esp32/sensor/status"
SYSTEM_STATUS_TOPIC = "home/esp32/system/status"

# Test tracking
test_results = {
    "callbacks_detected": 0,
    "relays_toggled": 0,
    "messages_received": 0
}

def on_connect(client, userdata, flags, rc):
    """Callback pour la connexion"""
    if rc == 0:
        print("✅ Connected to MQTT broker")
        print(f"   Broker: {BROKER_HOST}:{BROKER_PORT}")
        
        # Subscribe to all status topics
        client.subscribe(RELAY_STATUS_TOPIC)
        client.subscribe(INPUT_STATUS_TOPIC)
        client.subscribe(SENSOR_STATUS_TOPIC)
        client.subscribe(SYSTEM_STATUS_TOPIC)
        print("✓ Subscribed to status topics")
    else:
        print(f"❌ Connection failed with code {rc}")

def on_disconnect(client, userdata, rc):
    """Callback pour la déconnexion"""
    if rc != 0:
        print(f"⚠️  Unexpected disconnection: {rc}")

def on_message(client, userdata, msg):
    """Callback pour les messages reçus"""
    test_results["messages_received"] += 1
    
    print(f"\n📨 Message received #{test_results['messages_received']}:")
    print(f"   Topic: {msg.topic}")
    
    try:
        payload = json.loads(msg.payload.decode())
        print(f"   Payload (JSON): {json.dumps(payload, indent=6)}")
    except:
        print(f"   Payload: {msg.payload.decode()}")

def send_relay_command(client, relay, state):
    """Send a relay command"""
    command = f"{relay}:{'on' if state else 'off'}"
    print(f"\n📤 Sending command: {command}")
    client.publish(RELAY_CMD_TOPIC, command)
    test_results["relays_toggled"] += 1
    time.sleep(1)

def send_broadcast_command(client, state):
    """Send broadcast command to all relays"""
    command = f"ALL:{'on' if state else 'off'}"
    print(f"\n📤 Sending broadcast command: {command}")
    client.publish(RELAY_CMD_TOPIC, command)
    test_results["relays_toggled"] += 1
    time.sleep(1)

def run_test_sequence(client):
    """Run the test sequence"""
    print("\n" + "="*50)
    print("  MQTT CALLBACK TEST SEQUENCE")
    print("="*50)
    
    # Test 1: Individual relay commands
    print("\n🧪 Test 1: Individual Relay Commands")
    print("-" * 50)
    
    for relay in range(3):  # Test relays 0, 1, 2
        print(f"\n→ Relay {relay} ON")
        send_relay_command(client, relay, True)
        time.sleep(0.5)
        
        print(f"→ Relay {relay} OFF")
        send_relay_command(client, relay, False)
        time.sleep(0.5)
    
    # Test 2: Broadcast commands
    print("\n🧪 Test 2: Broadcast Commands")
    print("-" * 50)
    
    print("\n→ Turn ALL relays ON")
    send_broadcast_command(client, True)
    time.sleep(2)
    
    print("\n→ Turn ALL relays OFF")
    send_broadcast_command(client, False)
    time.sleep(2)
    
    # Test 3: Rapid sequence
    print("\n🧪 Test 3: Rapid Relay Toggling")
    print("-" * 50)
    
    for i in range(5):
        print(f"\n→ Toggle cycle {i+1}/5")
        send_relay_command(client, 0, True)
        send_relay_command(client, 0, False)
        time.sleep(0.5)
    
    print("\n" + "="*50)
    print("  TEST SEQUENCE COMPLETE")
    print("="*50)

def print_summary(client):
    """Print test summary"""
    print("\n" + "="*50)
    print("  TEST SUMMARY")
    print("="*50)
    print(f"\n📊 Results:")
    print(f"   Messages Received: {test_results['messages_received']}")
    print(f"   Relay Commands Sent: {test_results['relays_toggled']}")
    print(f"\n✅ If you see this and messages arrived on the ESP32,")
    print(f"   then MQTT callbacks are working!")
    print("="*50 + "\n")

def main():
    print("\n╔════════════════════════════════════════╗")
    print("║  ESP32 MQTT Callback Test Script      ║")
    print("║  Using 256dpi/arduino-mqtt            ║")
    print("╚════════════════════════════════════════╝\n")
    
    # Create MQTT client
    try:
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, client_id=CLIENT_ID)
    except (AttributeError, TypeError):
        client = mqtt.Client(client_id=CLIENT_ID)
    if BROKER_USER:
        client.username_pw_set(BROKER_USER, BROKER_PASSWORD)
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    
    try:
        # Connect to broker
        print(f"🔌 Connecting to broker: {BROKER_HOST}:{BROKER_PORT}")
        client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)
        
        # Start network loop
        client.loop_start()
        
        # Give time to connect
        time.sleep(2)
        
        if not client.is_connected():
            print("❌ Failed to connect to broker")
            return False
        
        # Run test sequence
        run_test_sequence(client)
        
        # Give time for callbacks to arrive
        print("\n⏳ Waiting for callbacks...")
        time.sleep(5)
        
        # Print summary
        print_summary(client)
        
        # Disconnect
        client.loop_stop()
        client.disconnect()
        
        return True
        
    except KeyboardInterrupt:
        print("\n⚠️  Test interrupted by user")
        client.loop_stop()
        client.disconnect()
        return False
    except Exception as e:
        print(f"\n❌ Error: {e}")
        client.loop_stop()
        client.disconnect()
        return False

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
