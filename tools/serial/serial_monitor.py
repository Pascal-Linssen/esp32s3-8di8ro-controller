#!/usr/bin/env python3
"""Simple serial monitor for ESP32.

Usage:
    python tools/serial/serial_monitor.py --port COM4 --baud 9600
"""

import argparse
import serial


def parse_args() -> argparse.Namespace:
        parser = argparse.ArgumentParser(description="Simple serial monitor for ESP32")
        parser.add_argument("--port", default="COM4", help="Serial port (e.g. COM4)")
        parser.add_argument("--baud", type=int, default=9600, help="Baud rate (default: 9600)")
        return parser.parse_args()

args = parse_args()

try:
    print(f"Connexion à {args.port} @ {args.baud} baud...")
    ser = serial.Serial(args.port, args.baud, timeout=1)
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
    print(f"  Vérifiez que {args.port} existe et n'est pas utilisé par un autre programme")
