# Implementation Plan: HTTP Web Server for Relay Control

## Status: v1.5 - Hardware Control Complete, HTTP Integration Pending

### What We Have (✅ VERIFIED)

**Hardware Control Functions:**
- ✅ I2C TCA9554 Relay Control (8x relays, SDA=42, SCL=41)
- ✅ Digital Inputs (8x inputs, pins 4-11, internal pullups)
- ✅ DHT22 Sensor (temperature/humidity)
- ✅ Ethernet W5500 (IP: 192.168.1.50, connected)
- ✅ Serial CLI Commands (relay on/off, test, help)

**Software Foundation:**
- ✅ PlatformIO build system (esp32-s3 target)
- ✅ All dependencies installed and working
- ✅ 2-second sensor polling loop
- ✅ 9600 baud serial interface

### Current Architecture

```
┌─────────────────────────────────┐
│       ESP32-S3 Main Loop        │
├─────────────────────────────────┤
│ • Ethernet.maintain()           │
│ • handleHttpConnections() [stub]│
│ • Serial CLI processing         │
│ • Sensor polling (2s)           │
└─────────────────────────────────┘
         ↓↓↓
    I2C TCA9554
    (8 Relays)
         ↓↓↓
    Digital Inputs
    (8 Inputs)
```

### The HTTP Challenge

**Problem:** `EthernetServer` class not available in Ethernet @ 2.0.2 for ESP32

**Why:** Different frameworks for different boards
- Arduino (ATmega) boards: Full EthernetServer support
- ESP32 boards: Needs AsyncWebServer or custom socket implementation

**Solutions Considered:**

1. **AsyncWebServer** (RECOMMENDED)
   - Pros: Full async HTTP, WebSocket support, easy REST API
   - Cons: Requires additional library, no hardware limitation
   - Status: Not installed yet

2. **Custom socket implementation**
   - Pros: Lightweight, works with W5500 directly
   - Cons: Complex, need raw socket handling
   - Status: Complex, low priority

3. **Simple UDP/TCP wrapper**
   - Pros: Minimal overhead
   - Cons: Very limited functionality
   - Status: Not practical

### Next Steps (PRIORITY ORDER)

#### PHASE 1: HTTP Server Setup (THIS WEEK)

1. **Add AsyncWebServer to dependencies**
   ```ini
   lib_deps =
     ...
     me-no-dev/ESPAsyncWebServer @ ^1.2.3
   ```

2. **Implement basic HTTP GET endpoints**
   ```
   GET /api/relays → JSON: [state of 8 relays]
   GET /api/inputs → JSON: [state of 8 inputs]
   GET /api/sensors → JSON: {temp, humidity}
   ```

3. **Implement HTTP POST endpoints for control**
   ```
   POST /api/relay/{id}/on
   POST /api/relay/{id}/off
   POST /api/relay/{id}/toggle
   ```

4. **Serve dashboard HTML page**
   ```
   GET / → getHtmlPage() as HTTP response
   ```

5. **Add auto-refresh capability**
   ```html
   <!-- JavaScript: poll /api/status every 2 seconds -->
   ```

#### PHASE 2: Advanced Features (NEXT WEEK)

1. **MQTT Integration**
   - Connect to broker: 192.168.1.200:1883
   - Publish: `waveshare/relay/[0-7]` = on/off
   - Subscribe: `waveshare/relay/[0-7]/cmd` = on/off/toggle

2. **WebSocket Support** (real-time updates)
   - Server-sent events or WebSocket
   - Eliminate polling, push updates from ESP32

3. **Home Assistant Integration**
   - MQTT discovery protocol
   - Auto-register devices in HA

4. **Modbus TCP Support**
   - Modbus server on port 502
   - Read coils (relays), discrete inputs (DIOs)

#### PHASE 3: Optional Enhancements

1. **SD Card Logging**
2. **CAN Bus Interface**
3. **Advanced Dashboard** (Charts, History)
4. **Firmware OTA Updates**

### Current Test Results

**Relay Control Test (Serial):**
```
✓ relay 0 on   → Relais 1: ON (TCA9554 @ 0x20 bit 0)
✓ relay 7 off  → Relais 8: OFF (TCA9554 @ 0x20 bit 7)
✓ test         → All 8 relays cycled successfully
```

**Input Status:**
```
✓ All 8 inputs reading correctly
✓ Input 6 physical detection verified (LOW state)
```

**Network Status:**
```
✓ Ethernet connected: 192.168.1.50
✓ Gateway: 192.168.1.1
✓ DNS: 8.8.8.8
```

### Code Files

**Main Implementation:**
- `src/main.cpp` (360 lines, fully functional hardware)
  - `setRelay()` → I2C write to TCA9554
  - `readInputs()` → GPIO read
  - `readSensors()` → DHT22 read
  - `getHtmlPage()` → HTML/CSS page (ready to serve)
  - `loop()` → Serial CLI + sensor polling

**Test Utilities:**
- `test_relays.py` → Python serial CLI for testing

**Documentation:**
- `docs/SESSION_3_PROGRESS.md` → This session's work
- `docs/wiring.md` → Pin definitions (verified)
- `docs/MQTT.md` → Broker credentials
- `docs/WEB_INTERFACE.md` → REST API design

### Hardware Pinout (VERIFIED)

```
SPI (W5500 Ethernet):
  SCK  = GPIO 15
  MISO = GPIO 14
  MOSI = GPIO 13
  CS   = GPIO 16
  RST  = GPIO 39
  IRQ  = GPIO 12

I2C (TCA9554 Relays @ 0x20):
  SDA  = GPIO 42   ✓ CORRECTED
  SCL  = GPIO 41   ✓ CORRECTED

Digital Inputs (pull-ups):
  DI0-7 = GPIO 4,5,6,7,8,9,10,11

Sensor:
  DHT22 = GPIO 40 (Temp/Humidity)
```

### Performance Metrics

- **Compilation:** 9-15 seconds
- **Upload:** ~20 seconds (via COM4 @ 460800 baud)
- **Boot:** ~2 seconds to ready state
- **Loop Rate:** 2 seconds (polling interval)
- **Memory:** 5.9% RAM used, 8.8% Flash used

### Compilation Status

```
✓ Last build: SUCCESS
✓ All libraries resolve correctly
✓ No compilation warnings or errors
✓ Firmware size: ~302KB / 3.3MB available
```

### Ready for Implementation

This system is **production-ready for hardware control**. Next phase is adding HTTP interface for remote access.

Estimated effort: **2-3 hours** for basic HTTP server + REST API
