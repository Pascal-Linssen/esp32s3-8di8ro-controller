#!/usr/bin/env python3
"""Test all serial ports for ESP32"""

import serial
import serial.tools.list_ports

print("=== Ports série disponibles ===\n")

ports = serial.tools.list_ports.comports()
if not ports:
    print("✗ Aucun port trouvé!")
    exit(1)

for port in ports:
    print(f"  {port.device}: {port.description}")

print("\n=== Test de connexion ===\n")

for port_info in ports:
    port = port_info.device
    try:
        print(f"Test {port}...", end=" ")
        ser = serial.Serial(port, 115200, timeout=1)
        
        # Envoyer une ligne vide pour réveiller l'ESP32
        ser.write(b"\r\n")
        import time
        time.sleep(0.5)
        
        data = ser.read(100)
        if data:
            print(f"✓ Réponse reçue!")
            print(f"  {data.decode('utf-8', errors='ignore')[:100]}")
        else:
            print("  (pas de réponse)")
        ser.close()
    except Exception as e:
        print(f"✗ {e}")
