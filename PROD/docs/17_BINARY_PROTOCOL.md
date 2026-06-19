# 17 — Binary Protocol (ÉTAPE 3: Fast WiFi)

> High-speed binary communication protocol for OpenRB ↔ ESP ↔ Frontend

## Overview

**Goal:** Replace JSON/HTTP with compact binary frames for **<100ms latency** and **50× throughput**.

**Key design:**
- 8–20 bytes per frame (vs 50+ bytes JSON)
- CRC8 checksum (error detection)
- Zero allocations (fixed-size buffers)
- Single protocol layer (no JSON intermediate)

---

## Frame Format

All frames follow this structure:

```
[FRAME_TYPE: 1 byte] [PAYLOAD: 0-6 bytes] [CHECKSUM: 1 byte]
Total: 2–8 bytes minimum
```

### Frame Types

| Type | Name | Direction | Payload Size | Example |
|------|------|-----------|--------------|---------|
| `0xC` | **COMMAND** | Host → Device | 2–6 bytes | Set frequency, START, STOP |
| `0xR` | **RESPONSE** | Device → Host | 1–6 bytes | ACK, DONE, ERROR |
| `0xS` | **STATUS** | Device → Host (stream) | 18 bytes | Live positions, forces, freq |

---

## Command Frame (0xC)

Sent from backend/ESP → OpenRB.

```
[0xC] [CMD_ID: 1] [ARG1–ARG5: 0-5 bytes] [CRC8: 1]
```

### Command IDs

| ID | Name | Args | Size | Purpose |
|----|------|------|------|---------|
| `0x01` | **START** | None | 0 | Begin oscillation cycle |
| `0x02` | **STOP** | None | 0 | Soft stop (hold position, maintain forces) |
| `0x03` | **HARD_RESET** | None | 0 | Force all tables to home (z=0), disable oscillation |
| `0x04` | **HOME** | None | 0 | Same as HARD_RESET (alias) |
| `0x10` | **SET_FREQ** | frequency (u8: 0–100 Hz) | 1 | Set oscillation frequency |
| `0x11` | **SET_FORCE** | sensor (u8: 1–4), target (u16 mV) | 3 | Set force target for one cell |
| `0x12` | **SET_FORCE_ALL** | target (u16 mV) | 2 | Set force target for all 4 cells |
| `0x20` | **GOTO** | table (u8: 1–4), position (u16: mm×10) | 3 | Move Dynamixel to absolute position |
| `0x30` | **MOTOR_BLINK** | motor_id (u8), duration_ms (u16) | 3 | ID-assistance: blink LED for N ms |
| `0x31` | **SCAN_DXL** | None | 0 | Force re-scan of Dynamixels, report IDs |
| `0x40` | **SET_RESISTANCE** | resistance_ohm (u16) | 2 | Set gain resistor (optional) |
| `0xF0` | **GET_STATUS** | None | 0 | Poll current state (triggers STATUS response) |

### Encoding Examples

**START:**
```
[0xC] [0x01] [CRC8]
Hex: C1 XX   (3 bytes)
```

**SET_FREQ 50 Hz:**
```
[0xC] [0x10] [50] [CRC8]
Hex: C1 10 32 XX   (4 bytes)
```

**SET_FORCE sensor=2, target=10.5 N = 10500 mV:**
```
[0xC] [0x11] [2] [0x29, 0x27] [CRC8]
       ↑      ↑  ↑ u16 LE: 10500 = 0x2729
Hex: C1 11 02 29 27 XX   (6 bytes)
```

**GOTO table=1, position=50 mm = 500 (mm×10):**
```
[0xC] [0x20] [1] [0xF4, 0x01] [CRC8]
       ↑      ↑  ↑ u16 LE: 500 = 0x01F4
Hex: C1 20 01 F4 01 XX   (6 bytes)
```

**HARD_RESET:**
```
[0xC] [0x03] [CRC8]
Hex: C1 03 XX   (3 bytes)
```

**STOP (soft hold):**
```
[0xC] [0x02] [CRC8]
Hex: C1 02 XX   (3 bytes)
```

---

## Response Frame (0xR)

Sent from OpenRB → ESP/Host (single response to a command).

```
[0xR] [RESULT: 1] [DATA: 0-6 bytes] [CRC8: 1]
```

### Result Codes

| Code | Meaning | Data | Purpose |
|------|---------|------|---------|
| `0x00` | **ACK** | None | Command accepted, execution starting |
| `0x01` | **DONE** | None | Command completed successfully |
| `0x80` | **ERROR_INVALID_CMD** | None | Unknown command ID |
| `0x81` | **ERROR_INVALID_ARG** | None | Argument out of range |
| `0x82` | **ERROR_CRC** | None | Checksum failed |
| `0x83` | **ERROR_DEVICE** | None | Hardware error (e.g., Dynamixel timeout) |

### Examples

**ACK for SET_FREQ:**
```
[0xR] [0x00] [CRC8]
Hex: R0 00 XX   (3 bytes)
```

**DONE for START:**
```
[0xR] [0x01] [CRC8]
Hex: R0 01 XX   (3 bytes)
```

**ERROR (invalid arg):**
```
[0xR] [0x81] [CRC8]
Hex: R0 81 XX   (3 bytes)
```

---

## Status Frame (0xS)

Sent continuously from OpenRB → ESP (every ~100 ms).

```
[0xS] [FREQ: 2 bytes LE] [POSITIONS: 8 bytes] [FORCES: 8 bytes] [CRC8: 1]
Total: 20 bytes
```

### Encoding

| Field | Type | Bytes | Range | Notes |
|-------|------|-------|-------|-------|
| Type | u8 | 1 | 0xS | Status frame marker |
| Frequency | u16 LE | 2 | 0–1000 | Hz × 10 (so 50 Hz = 500) |
| Position[0] | u16 LE | 2 | 0–10000 | mm × 10 (table 1) |
| Position[1] | u16 LE | 2 | 0–10000 | mm × 10 (table 2) |
| Position[2] | u16 LE | 2 | 0–10000 | mm × 10 (table 3) |
| Position[3] | u16 LE | 2 | 0–10000 | mm × 10 (table 4) |
| Force[0] | u16 LE | 2 | 0–50000 | mV (sensor 1) |
| Force[1] | u16 LE | 2 | 0–50000 | mV (sensor 2) |
| Force[2] | u16 LE | 2 | 0–50000 | mV (sensor 3) |
| Force[3] | u16 LE | 2 | 0–50000 | mV (sensor 4) |
| Checksum | u8 | 1 | 0–255 | CRC8 |

### Example

OpenRB state: freq=50 Hz, positions=[10.5, 11.2, 9.8, 10.1] mm, forces=[2.3, 2.4, 2.2, 2.3] N = [2300, 2400, 2200, 2300] mV

```
[0xS] [500 LE] [105, 112, 98, 101 LE] [2300, 2400, 2200, 2300 LE] [CRC8]
       0x01F4   0x69, 0x00, 0x70, 0x00, 0x62, 0x00, 0x65, 0x00   0xFC, 0x08, 0xA0, 0x09, 0xB8, 0x08, 0xAC, 0x09   XX

Hex dump:
S3 F4 01 69 00 70 00 62 00 65 00 FC 08 A0 09 B8 08 AC 09 XX
```

---

## Checksum (CRC8)

All frames include a **CRC8 checksum** (last byte).

### Algorithm

```cpp
uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0xFF;  // Initial value
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;  // Polynomial
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}
```

### Example

Frame: `[0xC] [0x01]` (START, no args)
```
data = [0xC, 0x01]
crc8([0xC, 0x01]) = 0xXX  (computed below)
Final frame: [0xC] [0x01] [0xXX]
```

---

## STOP vs HARD_RESET (Critical Distinction)

### **STOP (0x02) — Soft Stop**

**Intent:** Pause oscillation but **hold current position and force**.

**Behavior:**
- Oscillation frequency → 0
- Dynamixel tables → stay at current position
- Force control loop → active (tries to maintain target force)
- State: `STATE:STOPPED` (not `STOPPED`)
- User can resume with START (no need to recalibrate)

**Use case:** Pause during an experiment, will resume cleanly.

### **HARD_RESET (0x03) — Force Reset**

**Intent:** **Stop everything** immediately, zero out all positions.

**Behavior:**
- Oscillation frequency → 0
- Dynamixel tables → move to HOME (z = 0)
- Force control loop → disabled
- All setpoints (frequency, force targets) → cleared or reset to default
- State: `STATE:IDLE`
- After reset, user must reconfigure (SET_FREQ, SET_FORCE) before START

**Use case:** Emergency stop, reset to known state, cleanup after a failed experiment.

### **HOME (0x04) — Alias**

**Same as HARD_RESET** (for compatibility).

### Decision Logic (OpenRB firmware)

```cpp
void dispatch(const uint8_t* frame, size_t len) {
    uint8_t cmd = frame[1];  // Command ID
    
    if (cmd == 0x02) {  // STOP
        // Soft stop: pause oscillation, keep positions
        g_frequency = 0;
        g_oscillating = false;
        // Force control loop stays active!
        sendResponse(RESULT_ACK);
        
    } else if (cmd == 0x03 || cmd == 0x04) {  // HARD_RESET or HOME
        // Hard stop: go to HOME
        g_frequency = 0;
        g_oscillating = false;
        g_force_control_enabled = false;
        
        // Move all Dynamixels to home
        for (int i = 0; i < 4; i++) {
            dynamixel.setGoalPosition(table_ids[i], 0);
            g_positions[i] = 0;
            g_force_targets[i] = 0;
        }
        sendResponse(RESULT_DONE);
    }
}
```

---

## Layer Integration

### ESP (Gateway)

1. Receive binary command frame from WiFi (WebSocket or raw TCP)
2. Forward to OpenRB via UART1 @ 19200 baud
3. Receive response/status frames from UART1
4. Cache STATUS frames, push to WebSocket clients
5. Relay single RESPONSE frames back to command sender (ACK/ERROR)

### OpenRB (Executor)

1. Receive binary command frame from UART1
2. Validate checksum, parse command ID + args
3. Execute (set frequency, move Dynamixel, etc.)
4. Send RESPONSE frame back (ACK or ERROR)
5. **Continuously** emit STATUS frames every 100 ms (regardless of commands)

### Backend (Translator)

1. Receive REST command from React (`POST /api/command {command: "START"}`)
2. Translate to binary frame (`[0xC] [0x01] [CRC8]`)
3. Send to ESP via raw TCP (port 9000)
4. Wait for RESPONSE (ACK, 100ms timeout)
5. Return to React
6. **Status comes from WebSocket**, not from this response

### Frontend (UI)

1. Open WebSocket to ESP (`ws://192.168.4.1:8080/ws`)
2. On message: parse STATUS frame, update live position/force display
3. On command: send REST to backend, backend translates + sends to ESP
4. Real-time status updates (<50ms latency)

---

## Testing & Debugging

### Binary Frame Examples (Hex Dumps)

| Command | Hex | Notes |
|---------|-----|-------|
| START | `C1 01 XX` | 3 bytes |
| STOP | `C1 02 XX` | Soft hold |
| HARD_RESET | `C1 03 XX` | Emergency stop |
| SET_FREQ 50 Hz | `C1 10 32 XX` | 4 bytes |
| SET_FORCE sensor=1, 5.0 N | `C1 11 01 88 13 XX` | 6 bytes, 5000 mV LE |
| STATUS (example) | `S3 F4 01 69 00 70 00 62 00 65 00 FC 08 A0 09 B8 08 AC 09 XX` | 20 bytes |

### Decoder Tool

Use `tools/decode_frames.py` to parse binary frames from serial:

```bash
python tools/decode_frames.py --port /dev/ttyUSB0 --baud 19200
```

Output:
```
[12:34:56.123] COMMAND: START
[12:34:56.124] RESPONSE: ACK
[12:34:56.224] STATUS: freq=50.0 Hz, pos=[10.5, 11.2, 9.8, 10.1] mm, force=[2.3, 2.4, 2.2, 2.3] N
[12:34:56.324] STATUS: freq=50.0 Hz, pos=[10.5, 11.2, 9.8, 10.1] mm, force=[2.3, 2.4, 2.2, 2.3] N
```

---

## Changelog

### vs Phase 2 (JSON/HTTP)

| Aspect | Phase 2 | Phase 3 |
|--------|---------|---------|
| Protocol | JSON text | Binary frames |
| Frame size | 50–100 bytes | 3–20 bytes |
| Serialization | JSON → string | Direct bit packing |
| Baud rate | 9600 (SoftwareSerial) | 19200 (UART1) |
| Status updates | HTTP polling (700ms) | WebSocket push (100ms) |
| Command latency | 500ms–2s | <100ms (target) |
| Allocations | Frequent (String) | None (fixed buffers) |

---

## Future Enhancements

1. **Compression:** Add frame type for delta-encoded positions (if bandwidth is issue)
2. **Fragmentation:** For large responses, add fragment ID + reassembly
3. **Priority:** Command priority queue (STOP always first)
4. **Heartbeat:** Detect link dropout via periodic ACK/PING
