# Phase 1: ESP8266 WiFi REST API - Command Logging

## Overview

**Phase 1 (Current):** PC → NodeMCU WiFi REST API → USB Logging  
**Phase 2 (Future):** NodeMCU → OpenRB-150 via UART

In Phase 1, the ESP8266 acts as a simple WiFi gateway that:
- Receives REST API commands from the backend
- Logs them to USB serial port
- Returns success response
- Does NOT communicate with OpenRB yet (that's Phase 2)

```
Frontend (React)
    ↓ HTTP
Backend (FastAPI)
    ↓ HTTP
ESP8266 REST API
    ↓ USB Serial (115200)
Terminal/Log file
    
[OpenRB-150 communication comes in Phase 2]
```

## Quick Setup

### 1. Configure ESP8266 WiFi

Edit `hardware/machine_final/ESP8266/include/config.h`:

```cpp
#define WIFI_SSID "YourNetwork"
#define WIFI_PASSWORD "YourPassword"
#define AUTH_TOKEN "test_token_12345"
#define HTTP_PORT 8080
```

### 2. Upload Firmware

```bash
cd hardware/machine_final/ESP8266
platformio run -t upload -e nodemcuv2
platformio device monitor --baud 115200
```

Output:
```
╔════════════════════════════════════════╗
║  NodeMCU ESP8266 — WiFi REST Gateway   ║
║  Phase 1: Command Logging Only         ║
╚════════════════════════════════════════╝

[WiFi] Connecting to: YourNetwork
.......
[WiFi] ✓ Connected! IP: 192.168.1.100
[WiFi] RSSI: -45 dBm
[Server] HTTP server started on port 8080

[Boot] ✓ Ready to receive commands via REST API
[Boot] Endpoint: http://192.168.1.100:8080
[Boot] All commands will be logged to this USB port
```

**Copy the IP address!** (e.g., 192.168.1.100)

### 3. Configure Backend

Create `tools/config/setup.json`:

```json
{
  "wifi": {
    "ssid": "YourNetwork",
    "password": "YourPassword"
  },
  "nodeMcu": {
    "ip": "192.168.1.100",
    "port": 8080,
    "key": "test_token_12345"
  }
}
```

Replace IP with the one from step 2!

### 4. Start Backend

```bash
cd src
./start.sh
```

### 5. Test via cURL

```bash
# Test status
curl -H "Authorization: Bearer test_token_12345" \
  http://192.168.1.100:8080/api/status

# Send a command
curl -X POST \
  -H "Authorization: Bearer test_token_12345" \
  -H "Content-Type: application/json" \
  -d '{"command":"HIGH"}' \
  http://192.168.1.100:8080/api/command
```

### 6. Watch USB Logs

Keep terminal with serial monitor open:

```bash
cd hardware/machine_final/ESP8266
platformio device monitor --baud 115200
```

You'll see logs like:
```
[REST] Received command: {"command":"HIGH"}
[REST] GET /api/status
[CMD] Processing: HIGH
[LOG] Command #1: HIGH
[REST] Command response sent
```

## API Endpoints (Phase 1)

### GET /api/status

Get system status.

**Request:**
```bash
curl -H "Authorization: Bearer test_token_12345" \
  http://192.168.1.100:8080/api/status
```

**Response:**
```json
{
  "status": "ok",
  "command_count": 5,
  "uptime_ms": 12345,
  "rssi": -45,
  "version": "1.0.0-phase1",
  "ip": "192.168.1.100"
}
```

### POST /api/command

Send a command (logged to USB, not executed yet).

**Request:**
```bash
curl -X POST \
  -H "Authorization: Bearer test_token_12345" \
  -H "Content-Type: application/json" \
  -d '{"command":"HIGH"}' \
  http://192.168.1.100:8080/api/command
```

**Response:**
```json
{
  "result": "success",
  "command": "HIGH",
  "timestamp": 12345,
  "command_number": 1
}
```

**Available commands:**
- `HIGH` - Placeholder (Phase 2: set GPIO HIGH)
- `LOW` - Placeholder (Phase 2: set GPIO LOW)
- `STATUS` - Placeholder (Phase 2: get current state)

## Test Script

```bash
python tools/test_esp8266_api.py \
  --ip 192.168.1.100 \
  --port 8080 \
  --token test_token_12345
```

## Frontend Usage

1. Open http://localhost:5173
2. See "WiFi Interfaces" section (purple)
3. Click interface → select it
4. Click "Connect"
5. Use Control Panel - commands logged to ESP8266 USB

## USB Serial Output Example

```
[REST] Received command: {"command":"HIGH"}
[REST] GET /api/status
[CMD] Processing: HIGH
[LOG] Command #1: HIGH
[REST] Command response sent

[REST] Received command: {"command":"LOW"}
[CMD] Processing: LOW
[LOG] Command #2: LOW
[REST] Command response sent

[REST] GET /api/status
```

## Next Steps (Phase 2)

1. Add SoftwareSerial for OpenRB communication
2. Change `handleCommand()` to send to OpenRB via UART
3. Parse OpenRB responses
4. Return actual hardware state instead of placeholder

## File Structure for Phase 1

```
hardware/machine_final/ESP8266/
├── include/
│   └── config.h              ← Edit WiFi credentials here
├── src/
│   └── main.cpp              ← REST API server (simplified, no OpenRB)
└── platformio.ini            ← Build config
```

## Debugging

### Check connectivity
```bash
ping 192.168.1.100
```

### Test API directly
```bash
curl http://192.168.1.100:8080/api/status
# Should get 401 Unauthorized (no token)

curl -H "Authorization: Bearer test_token_12345" \
  http://192.168.1.100:8080/api/status
# Should get status JSON
```

### Serial monitor
Keep this running to see all logs:
```bash
platformio device monitor --baud 115200
```

### Check backend logs
```bash
tail -f /tmp/machine_backend.log
```

## Security Notes

- Change `AUTH_TOKEN` from `test_token_12345` to something unique
- config.h is compiled into firmware (secure)
- setup.json is gitignored (secure)
- For production, consider HTTPS

## Troubleshooting

| Problem | Solution |
|---------|----------|
| "Connection refused" | Check ESP8266 is powered and on WiFi |
| "Unauthorized" error | Verify AUTH_TOKEN matches in both places |
| No USB logs | Check serial port settings (115200 baud) |
| WiFi won't connect | Check SSID/password (case-sensitive) |
| Commands not being sent | Verify backend can reach ESP8266 IP |

## Summary

Phase 1 is complete when:
- ✓ ESP8266 connects to WiFi
- ✓ Can send commands via REST API
- ✓ Commands are logged to USB serial
- ✓ Frontend can see WiFi interface and connect
- ✓ Commands appear in serial monitor

Then Phase 2 adds OpenRB integration!
