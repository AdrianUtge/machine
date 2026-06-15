# ESP8266 WiFi Configuration Guide

## Overview

The ESP8266 (NodeMCU v3) acts as a WiFi gateway between the React frontend and the OpenRB-150 controller. It exposes a REST API that the backend can call to send commands via WiFi instead of serial.

```
Frontend (React)
    ↓ HTTP requests
Backend (Python FastAPI)
    ↓ HTTP requests
ESP8266 WiFi Gateway
    ↓ Serial UART (115200 baud)
OpenRB-150 Controller
    ↓ GPIO control
Motor/Force Sensor
```

## Hardware Setup

### Connections

**ESP8266 (NodeMCU v3) → OpenRB-150**

| ESP8266 Pin | GPIO | OpenRB Pin | Function |
|------------|------|-----------|----------|
| D7 | GPIO13 | RX | Receive commands |
| D8 | GPIO15 | TX | Send responses |
| GND | - | GND | Ground |
| D0 | GPIO16 | A1 | Control signal (optional test) |
| D4 | GPIO2 | - | LED indicator |

### Pinout Reference

```
NodeMCU v3 Pinout:
┌─────────────────────┐
│ USB  ┌───────────┐  │
│  ┌──┤           ├──┐│
│  │  │ ESP8266   │  ││
│  │  │           │  ││
│  │  └───────────┘  ││
│  │                 ││
│  D8 D7 .... D0 D4  ││
│  15 13            2 ││
└─────────────────────┘
```

## Software Configuration

### Step 1: Configure WiFi on ESP8266

Edit `hardware/machine_final/ESP8266/include/config.h`:

```cpp
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASSWORD "YourNetworkPassword"
#define AUTH_TOKEN "your_secret_bearer_token"
#define HTTP_PORT 8080
```

### Step 2: Upload Firmware to ESP8266

Using PlatformIO:

```bash
cd hardware/machine_final/ESP8266

# Compile and upload
platformio run -t upload -e nodemcuv2

# Monitor serial output
platformio device monitor --baud 115200
```

Expected output:
```
=== NodeMCU ESP8266 — WiFi REST Gateway ===
[Serial] OpenRB serial initialized at 115200 baud
[WiFi] Connecting to YourNetworkName
....
[WiFi] Connected! IP: 192.168.1.100
[WiFi] RSSI: -45
[Server] HTTP server started on port 8080
[Boot] Ready to receive commands via REST API
```

### Step 3: Configure Backend

Create `tools/config/setup.json`:

```json
{
  "wifi": {
    "ssid": "YourNetworkName",
    "password": "YourNetworkPassword",
    "timeout": 10
  },
  "nodeMcu": {
    "ip": "192.168.1.100",
    "port": 8080,
    "key": "your_secret_bearer_token",
    "protocol": "http",
    "timeout": 5
  },
  "serial": {
    "port": "auto",
    "baudrate": 115200,
    "timeout": 1
  }
}
```

**Important:** The `nodeMcu.ip` must match the IP assigned to your ESP8266 by your router. Check your router's DHCP client list or look at the serial monitor output.

### Step 4: Start the System

```bash
cd src
./start.sh
```

This will start:
- Backend API on http://127.0.0.1:8000
- Frontend on http://localhost:5173

### Step 5: Connect via Frontend

1. Open http://localhost:5173
2. You should see "WiFi Interfaces" section (purple)
3. Select your ESP8266's interface (e.g., "en0", "wlan0")
4. Click "Connect"
5. You should see "Connected" message

## API Endpoints

### GET /api/status

Get current system status.

**Request:**
```bash
curl -H "Authorization: Bearer your_secret_bearer_token" \
  http://192.168.1.100:8080/api/status
```

**Response:**
```json
{
  "status": "ok",
  "pin_state": "LOW",
  "openrb_connected": true,
  "uptime_ms": 12345,
  "rssi": -45,
  "version": "1.0.0",
  "ip": "192.168.1.100"
}
```

### POST /api/command

Send a command to control GPIO.

**Request:**
```bash
curl -X POST \
  -H "Authorization: Bearer your_secret_bearer_token" \
  -H "Content-Type: application/json" \
  -d '{"command":"HIGH"}' \
  http://192.168.1.100:8080/api/command
```

**Commands:**
- `HIGH` - Set GPIO to HIGH (3.3V)
- `LOW` - Set GPIO to LOW (0V)
- `STATUS` - Get current GPIO state

**Response:**
```json
{
  "result": "success",
  "command": "HIGH",
  "pin_state": "HIGH",
  "timestamp": 12345
}
```

## Debugging

### Check WiFi Connection

```bash
# Make sure your computer is on the same network as ESP8266
ping 192.168.1.100

# Should respond with ICMP replies
```

### Check API Endpoints

```bash
# Get status
curl -H "Authorization: Bearer your_secret_bearer_token" \
  http://192.168.1.100:8080/api/status

# Send command
curl -X POST \
  -H "Authorization: Bearer your_secret_bearer_token" \
  -H "Content-Type: application/json" \
  -d '{"command":"HIGH"}' \
  http://192.168.1.100:8080/api/command
```

### Serial Monitor

Watch ESP8266 logs:

```bash
cd hardware/machine_final/ESP8266
platformio device monitor --baud 115200
```

Look for:
- `[WiFi] Connected!` - WiFi connected successfully
- `[REST] Command: HIGH` - Command received
- `[OpenRB] ...` - OpenRB responses

### Backend Logs

```bash
tail -f /tmp/machine_backend.log

# Or enable debug logging:
DEBUG=1 python -m uvicorn api:app --reload
```

## Troubleshooting

### "Not on correct WiFi network"
- Verify your computer is connected to the same WiFi network
- Check SSID in setup.json matches your network name (case-sensitive!)
- Run `airport -I` (macOS) or check WiFi settings

### "Failed to connect to NodeMCU"
- Verify ESP8266 is powered on
- Check IP address with: `ping 192.168.1.100`
- Verify port (default 8080) is correct
- Check Bearer token matches

### "Timeout connecting to ESP8266"
- Verify computer and ESP8266 are on same subnet
- Check router firewall isn't blocking port 8080
- Increase timeout in setup.json

### ESP8266 Won't Connect to WiFi
- Check SSID and password in config.h
- Verify WiFi network is 2.4GHz (not 5GHz - ESP8266 doesn't support 5GHz)
- Increase warmup delay in setup(): `delay(2000)` instead of `delay(100)`

### OpenRB Not Responding
- Check serial connections (D7/D8 pins)
- Verify OpenRB is powered and receiving serial data
- Try direct serial connection to verify OpenRB is working

## Serial Protocol (OpenRB-150)

ESP8266 sends simple ASCII commands over serial:

```
H     → GPIO HIGH (3.3V)
L     → GPIO LOW (0V)
S     → Get STATUS
```

OpenRB responds with ASCII messages (e.g., "GPIO0 → HIGH\n")

## Performance

- WiFi: 100-500ms latency (network dependent)
- Serial: <10ms latency
- REST API overhead: ~50-100ms per request
- Typical end-to-end latency: 150-600ms

## Security Notes

1. **Change the default Bearer token** in config.h
2. **Use HTTPS** for production (requires certificate on ESP8266)
3. **Firewall** - Restrict port 8080 to local network only
4. **Password** - Use a strong WiFi password
5. **Secrets** - setup.json is in .gitignore, don't commit it

## Advanced: OTA Updates

To update ESP8266 firmware over-the-air, add ArduinoOTA to the project:

```cpp
#include <ArduinoOTA.h>

void setup() {
    // ... existing setup ...
    
    ArduinoOTA.begin();
}

void loop() {
    ArduinoOTA.handle();
    // ... rest of loop ...
}
```

Then upload with: `platformio run -t upload --upload-port 192.168.1.100`

## References

- [ESP8266 Arduino Core](https://github.com/esp8266/Arduino)
- [WebServer Documentation](https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/server.html)
- [ArduinoJson](https://arduinojson.org/)
- [OpenRB-150 Documentation](./../../hardware/machine_final/OPENRB150/)
