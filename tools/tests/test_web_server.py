#!/usr/bin/env python3
import socket
import time

# Test connection to ESP32-S3 HTTP server
esp32_ip = "192.168.1.50"
esp32_port = 80

try:
    print(f"Connecting to {esp32_ip}:{esp32_port}...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    result = sock.connect_ex((esp32_ip, esp32_port))
    
    if result == 0:
        print("✅ Connection successful!")
        
        # Send HTTP request
        request = b"GET /api/status HTTP/1.1\r\nHost: 192.168.1.50\r\nConnection: close\r\n\r\n"
        sock.sendall(request)
        print("📤 Sent: GET /api/status")
        
        # Receive response
        response = b""
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response += chunk
            print(f"📥 Received {len(chunk)} bytes")
        
        print("\n=== HTTP Response ===")
        print(response.decode('utf-8', errors='ignore'))
    else:
        print(f"❌ Connection failed (error code: {result})")
        print("   Make sure ESP32-S3 is powered and connected to network")
    
    sock.close()
except Exception as e:
    print(f"❌ Error: {e}")
