# CHANGELOG

## v1.5 - Session 3 (Dec 16, 2025, 10:45 AM)

### 🎯 BREAKTHROUGH: I2C TCA9554 Relay Control Fully Operational

#### Major Fixes
- **CRITICAL: Fixed I2C Pin Configuration**
  - Old (WRONG): SDA=21, SCL=22 (generic ESP32 defaults)
  - New (CORRECT): SDA=42, SCL=41 (Waveshare official per docs/wiring.md)
  - Impact: TCA9554 relay control now works perfectly

#### Changes to `src/main.cpp`
1. **Line 202-226: Updated I2C Initialization**
   - `Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN)` using corrected pins 42,41
   - Added `Wire.setClock(100000)` for stable I2C communication
   - Improved TCA9554 register initialization (config, output, IO settings)
   - Added diagnostic messages for I2C setup

2. **Line 300-347: Implemented Serial CLI Interface**
   - Added command parser in `loop()` for real-time relay control
   - Commands supported:
     - `help` - Show available commands
     - `relay X on/off` - Control relay X (0-7)
     - `test` - Cycle all relays for validation
   - Added graceful error handling

3. **Line 193-200: HTTP Server Stub**
   - Updated with better comments
   - Documented EthernetServer limitation
   - Ready for AsyncWebServer integration

#### Hardware Validation
- ✅ All 8 relays independently controllable via I2C
- ✅ All 8 digital inputs reading correctly
- ✅ Relay state tracking working (relay ON = bit 0, OFF = bit 1)
- ✅ Zero I2C communication errors

#### New Test Utilities
- **test_relays.py** - Python serial CLI script
  - Tests all relay commands
  - Validates I2C communication
  - Outputs formatted results

#### Documentation Created
1. **SESSION_3_PROGRESS.md** - Complete session summary
2. **IMPLEMENTATION_PLAN.md** - Roadmap for HTTP/MQTT integration
3. **Updated README.md** - Current status and capabilities

#### No Breaking Changes
- All existing functionality preserved
- Backward compatible with session 1-2 code
- Improved stability and diagnostics

#### Known Limitations (Still Present)
- HTTP server interface pending (AsyncWebServer library needed)
- DHT22 reading 0.0°C/0.0% (sensor may not be physically connected)
- No MQTT or Web interface yet (planned for session 4+)

#### Memory & Performance
- RAM: 5.9% used (excellent)
- Flash: 8.8% used (plenty of room for HTTP/MQTT)
- Compilation time: 9-20 seconds
- No reboot loops or instability

#### Build Information
- Compiler: xtensa-esp32s3-elf-g++
- Framework: Arduino 3.20017.241212
- Board: esp32-s3-devkitc-1
- All dependencies resolve correctly

### Testing Completed
- ✅ Serial CLI commands (relay on/off/test)
- ✅ I2C TCA9554 communication
- ✅ Digital input detection
- ✅ Ethernet connectivity
- ✅ System stability (no crashes)

### Next Session (v1.6+)
1. Add AsyncWebServer for HTTP interface
2. Implement REST API endpoints
3. Serve dashboard HTML page
4. Test web interface from browser
5. Begin MQTT integration

---

## v1.0 - Session 1 (Dec 15, 2025)

### Initial System Bring-Up
- ✅ ESP32-S3 boot and serial communication
- ✅ SPI initialization for W5500 Ethernet
- ✅ Ethernet connectivity and static IP
- ✅ Digital I/O configuration
- ✅ DHT22 sensor interface
- ✅ I2C bus initialization (pins 21, 22 - later corrected)
- ✅ TCA9554 basic communication
- ✅ HTML page generation

### Session 1 Challenges
- ❌ WebServer.h caused lwip conflicts (reboot loops)
- ❌ Resolved: Removed WebServer.h, using direct socket approach
- ❌ I2C pins were incorrect (21,22 instead of 42,41)

### Session 1 Result
- System stable with basic hardware detection
- Ethernet W5500 detected
- 8 relays + 8 inputs initialized
- Ready for I2C relay control (needed pin fix)

---

## Version Numbering

- **v1.0**: Hardware bring-up, basic functionality
- **v1.5**: I2C relay control verified working
- **v2.0** (planned): HTTP web server + REST API
- **v2.5** (planned): MQTT integration
- **v3.0** (planned): Full Home Assistant integration

---

## Key Files Modified

| File | Session | Changes |
|------|---------|---------|
| src/main.cpp | 3 | I2C pins corrected, serial CLI added, HTTP stub updated |
| README.md | 3 | Updated feature list, test results, status |
| docs/SESSION_3_PROGRESS.md | 3 | Created - complete summary |
| docs/IMPLEMENTATION_PLAN.md | 3 | Created - roadmap for next phases |
| test_relays.py | 3 | Created - Python test utility |

---

**Latest Version**: v1.5  
**Last Updated**: Dec 16, 2025, 10:45 AM UTC  
**Status**: ✅ Hardware Control Operational - Ready for HTTP Interface
