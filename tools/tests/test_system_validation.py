#!/usr/bin/env python3
"""
Validate ESP32 via MQTT as a workaround while HTTP interface is being implemented
"""

import paho.mqtt.client as mqtt
import json
import time
import sys

BROKER_IP = "192.168.1.200"
BROKER_PORT = 1883
MQTT_USER = "pascal"
MQTT_PASS = "123456"

def on_message(client, userdata, msg):
    print(f"[MQTT] {msg.topic}: {msg.payload.decode()}")

def main():
    print("=" * 60)
    print("ESP32-S3 System Validation via MQTT")
    print("=" * 60)
    
    client = mqtt.Client()
    client.username_pw_set(MQTT_USER, MQTT_PASS)
    client.on_message = on_message
    
    print(f"\nConnecting to {BROKER_IP}:{BROKER_PORT}...")
    client.connect(BROKER_IP, BROKER_PORT, 60)
    client.loop_start()
    
    # Subscribe to system status topics
    client.subscribe("sensor/status")
    client.subscribe("relay/status")
    client.subscribe("relay/input")
    client.subscribe("system/info")
    
    print("Subscribed to status topics (listening for 5 seconds)...")
    time.sleep(2)
    
    # Test relay commands via MQTT
    print("\n[TEST] Relay control via MQTT...")
    
    for i in range(3):
        print(f"\n  Relay {i}: ON")
        client.publish("relay/cmd", f"{i}:on")
        time.sleep(1)
        
        print(f"  Relay {i}: OFF")
        client.publish("relay/cmd", f"{i}:off")
        time.sleep(1)
    
    print("\n[TEST] All relays ON")
    client.publish("relay/cmd", "ALL:on")
    time.sleep(1)
    
    print("[TEST] All relays OFF")
    client.publish("relay/cmd", "ALL:off")
    time.sleep(1)
    
    print("\n[LISTENING] System status (10 seconds)...")
    time.sleep(10)
    
    client.loop_stop()
    client.disconnect()
    
    print("\n" + "=" * 60)
    print("✓ Validation complete")
    print("=" * 60)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nAborted")
        sys.exit(0)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
