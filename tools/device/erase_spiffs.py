#!/usr/bin/env python3
"""
Efface le SPIFFS complet de l'ESP32
"""

import subprocess
import sys

port = "COM4"
print(f"Effacement du SPIFFS sur le port {port}...")

# Utiliser esptool pour effacer la partition SPIFFS
# Sur ESP32-S3: SPIFFS est généralement à l'adresse 0x200000 (2MB) avec une taille de 1.4MB

result = subprocess.run([
    "python", "-m", "esptool", 
    "--chip", "esp32s3",
    "--port", port,
    "--baud", "921600",
    "erase_region", "0x200000", "0x160000"  # SPIFFS: 0x200000 + 1.4MB
], capture_output=True, text=True)

print(result.stdout)
if result.returncode == 0:
    print("✓ SPIFFS effacé avec succès")
else:
    print("✗ Erreur lors de l'effacement:", result.stderr)
    sys.exit(1)

# Attendre un peu et redémarrer
print("Redémarrage de l'ESP32...")
subprocess.run([
    "python", "-m", "esptool",
    "--chip", "esp32s3", 
    "--port", port,
    "read_mac"
], capture_output=True)

print("✓ SPIFFS réinitialisé, redémarrage en cours")
