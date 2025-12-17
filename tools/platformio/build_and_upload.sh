#!/bin/bash
# Script pour compiler et uploader

cd /c/Users/Pascal/Desktop/esp32s3_8di8ro_full

echo "🔨 Compilation du code ESP32..."
pio run

echo ""
echo "📤 Upload du firmware..."
pio run -t upload

echo ""
echo "📺 Ouverture du moniteur serial (Ctrl+C pour arrêter)"
pio device monitor -b 115200
