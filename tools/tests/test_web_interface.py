#!/usr/bin/env python3
"""
Test the ESP32 HTTP REST API web interface
Tests all relay control endpoints and status endpoint
"""

import requests
import json
import time
import sys

# ESP32 IP address and port
ESP32_IP = "192.168.1.50"
BASE_URL = f"http://{ESP32_IP}"

def test_status():
    """Test /api/status endpoint"""
    print("\n[TEST] Getting system status...")
    try:
        response = requests.get(f"{BASE_URL}/api/status", timeout=2)
        if response.status_code == 200:
            data = response.json()
            print("✓ Status endpoint working")
            print(f"  Relays: {data.get('r', [])}")
            print(f"  Inputs: {data.get('i', [])}")
            print(f"  Temp: {data.get('t', 0):.1f}°C")
            print(f"  Humidity: {data.get('h', 0):.1f}%")
            print(f"  MQTT: {data.get('m', False)}")
            return data
        else:
            print(f"✗ Status failed: {response.status_code}")
            return None
    except Exception as e:
        print(f"✗ Error: {e}")
        return None

def test_relay_control(relay_num, action):
    """Test relay control endpoint"""
    print(f"\n[TEST] Relay {relay_num} - {action.upper()}")
    try:
        response = requests.get(f"{BASE_URL}/api/r/{relay_num}/{action}", timeout=2)
        if response.status_code == 200:
            print(f"✓ Relay {relay_num} command sent")
            # Check status to verify
            time.sleep(0.5)
            status = test_status()
            if status:
                state = status['r'][relay_num]
                expected = (action == '1') if action != 't' else None
                if expected is not None:
                    match = "✓" if state == expected else "✗"
                    print(f"  {match} Relay state: {state} (expected {expected})")
            return True
        else:
            print(f"✗ Failed: {response.status_code}")
            return False
    except Exception as e:
        print(f"✗ Error: {e}")
        return False

def test_all_relays_control(action):
    """Test all relays control endpoint"""
    print(f"\n[TEST] All relays - {action.upper()}")
    try:
        response = requests.get(f"{BASE_URL}/api/r/all/{action}", timeout=2)
        if response.status_code == 200:
            print(f"✓ All relays command sent")
            # Check status
            time.sleep(0.5)
            status = test_status()
            if status:
                states = status['r']
                expected = (action == '1') if action != 't' else None
                if expected is not None:
                    match_count = sum(1 for s in states if s == expected)
                    print(f"  ✓ Relays in expected state: {match_count}/8")
            return True
        else:
            print(f"✗ Failed: {response.status_code}")
            return False
    except Exception as e:
        print(f"✗ Error: {e}")
        return False

def main():
    print("=" * 60)
    print("ESP32-S3 Web Interface REST API Test")
    print("=" * 60)
    
    # Test initial status
    status = test_status()
    if not status:
        print("\n✗ Cannot connect to ESP32")
        sys.exit(1)
    
    print("\n[TESTING] Individual relay control...")
    
    # Test turning relays ON one by one
    for i in range(8):
        test_relay_control(i, '1')
        time.sleep(0.3)
    
    # Turn all OFF
    test_all_relays_control('0')
    time.sleep(0.5)
    
    # Test turning relays ON one by one (different test)
    for i in range(8):
        test_relay_control(i, '0')
        time.sleep(0.3)
    
    # Test toggle
    print("\n[TESTING] Toggle operations...")
    test_relay_control(0, 't')
    time.sleep(0.5)
    test_relay_control(0, 't')  # Should be back to original
    
    # Test all toggle
    test_all_relays_control('t')
    time.sleep(0.5)
    test_all_relays_control('t')  # Toggle back
    
    # Final status
    print("\n[FINAL] System status:")
    test_status()
    
    print("\n" + "=" * 60)
    print("✓ All tests completed!")
    print("=" * 60)

if __name__ == "__main__":
    main()
