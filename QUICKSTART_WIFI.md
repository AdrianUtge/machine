# WiFi Setup Quick Start Guide

## TL;DR (5 minutes)

### 1. Configure ESP8266 WiFi
```bash
nano hardware/machine_final/ESP8266/include/config.h
```

Change these 3 lines:
```cpp
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASSWORD "YourNetworkPassword"
#define AUTH_TOKEN "your_secret_token_here"
```

### 2. Upload to ESP8266
```bash
cd hardware/machine_final/ESP8266
platformio run -t upload -e nodemcuv2
platformio device monitor --baud 115200
```

Wait for output:
```
[WiFi] Connected! IP: 192.168.x.x
```

Copy the IP address!

### 3. Configure Backend
Create `tools/config/setup.json`:
```json
{
  "wifi": {
    "ssid": "YourNetworkName",
    "password": "YourNetworkPassword"
  },
  "nodeMcu": {
    "ip": "192.168.x.x",
    "port": 8080,
    "key": "your_secret_token_here"
  }
}
```

Replace `192.168.x.x` with the IP from step 2!

### 4. Start System
```bash
cd src
./start.sh
```

### 5. Use It
1. Open http://localhost:5173
2. You'll see "WiFi Interfaces" (purple section)
3. Click the interface → becomes highlighted
4. Click "Connect"
5. See "Connected" message
6. Use Control Panel to send commands!

## Testing

```bash
# Verify ESP8266 is reachable
ping 192.168.1.100

# Test API directly
curl -H "Authorization: Bearer your_secret_token_here" \
  http://192.168.1.100:8080/api/status

# Run test script
python tools/test_esp8266_api.py
```

## Troubleshooting

**"Failed to connect to NodeMCU"**
- Check IP address is correct
- Verify ESP8266 is on same WiFi network
- Try: `ping 192.168.1.100`

**"Not on correct WiFi network"**
- Your computer must be on the SAME WiFi as ESP8266
- Check Settings → WiFi
- Reconnect if needed

**"Timeout"**
- Check port 8080 is not blocked by firewall
- Verify IP address from serial monitor

**ESP8266 won't connect to WiFi**
- Check SSID spelling (case-sensitive!)
- Verify 2.4GHz network (ESP8266 doesn't support 5GHz)
- Check password is correct

## Full Documentation

For complete setup guide, hardware diagrams, API documentation, and advanced options:

👉 **[Read: docs/ESP8266_WIFI_SETUP.md](docs/ESP8266_WIFI_SETUP.md)**

## Hardware Connections

```
ESP8266 NodeMCU v3        OpenRB-150
─────────────────         ──────────
D7 (GPIO13) ────────────→ RX
D8 (GPIO15) ────────────→ TX  
GND ────────────────────→ GND
```

If you need help, check the full documentation!
