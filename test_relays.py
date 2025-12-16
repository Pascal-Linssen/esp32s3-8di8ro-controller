#!/usr/bin/env python3
"""Test script for relay control via serial port"""

import serial
import time
import sys

def main():
    try:
        # Open serial port
        ser = serial.Serial('COM4', 9600, timeout=1)
        print(f"✓ Connected to {ser.port} at {ser.baudrate} baud")
        time.sleep(2)  # Wait for board to initialize
        
        # Test commands
        commands = [
            ("help", 1),
            ("relay 0 on", 2),
            ("relay 0 off", 2),
            ("relay 1 on", 2),
            ("relay 1 off", 2),
            ("relay 7 on", 2),
            ("relay 7 off", 2),
            ("test", 5),  # Test command takes longer
        ]
        
        for cmd, wait_time in commands:
            print(f"\n>>> Sending: {cmd}")
            ser.write((cmd + '\n').encode())
            time.sleep(0.1)
            
            # Read response
            time.sleep(wait_time)
            response = ""
            while ser.in_waiting > 0:
                response += ser.read(1).decode('utf-8', errors='ignore')
            
            if response:
                print(response.strip())
        
        ser.close()
        print("\n✓ Test completed")
        
    except serial.SerialException as e:
        print(f"✗ Serial error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"✗ Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
