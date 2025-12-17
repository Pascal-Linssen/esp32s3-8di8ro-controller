#!/usr/bin/env python3
"""Simple serial monitor for ESP32"""

import serial
import sys

PORT = "COM4"
BAUD = 115200

try:
    print(f"Connexion à {PORT} @ {BAUD} baud...")
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print("✓ Connecté!\n")
    print("=== CONSOLE SÉRIE ESP32 ===\n")
    
    while True:
        if ser.in_waiting:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(line)
except KeyboardInterrupt:
    print("\n\n[INFO] Arrêt...")
    ser.close()
except Exception as e:
    print(f"✗ Erreur: {e}")
    print(f"  Vérifiez que COM4 existe et n'est pas utilisé par un autre programme")
