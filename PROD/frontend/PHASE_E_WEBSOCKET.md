# Phase E: WebSocket Real-Time Status Hook

## Overview

Phase E replaces HTTP polling (`/api/status` @ 200ms) with direct WebSocket streaming of binary STATUS frames (0xS) for real-time data with <50ms latency.

## Architecture

```
┌─────────────────┐
│  OpenRB         │
│  (Firmware)     │
└────────┬────────┘
         │ Serial @ 19200 (UART1)
         │ Emits 0xS STATUS frames every ~100ms
         │
┌────────▼────────┐
│  ESP8266        │
│  (:8080/ws)     │
│  Broadcasts WS  │
└────────┬────────┘
         │ WebSocket
         │
┌────────▼──────────────────────────┐
│  Frontend (React)                  │
│                                    │
│  useLiveStatus(esp_ip)             │
│    └─> Connects ws://[ip]:8080/ws  │
│    └─> Parses 0xS binary frames    │
│    └─> Updates React state         │
│    └─> Auto-reconnect on error     │
│    └─> Fallback to HTTP if WS fails│
│                                    │
│  Component State:                  │
│    {frequency, positions, forces}  │
│                                    │
│  UI Latency: <50ms (vs 200ms poll) │
└────────────────────────────────────┘
```

## Hook Usage

```typescript
import { useLiveStatus } from './hooks/useLiveStatus';

export default function App() {
  const liveStatus = useLiveStatus('192.168.4.1');

  return (
    <div>
      <p>Frequency: {liveStatus.frequency} Hz</p>
      <p>Position 0: {liveStatus.positions?.[0]} mm</p>
      <p>Force 0: {liveStatus.forces?.[0]} mV</p>
      <p>Connected: {liveStatus.isConnected}</p>
      <p>Error: {liveStatus.error}</p>
    </div>
  );
}
```

## Return Type

```typescript
interface UseLiveStatusReturn {
  frequency: number | null;      // Hz (from protocol value / 10)
  positions: number[] | null;    // mm (4 values, protocol value / 10 each)
  forces: number[] | null;       // mV (4 values, protocol value as-is)
  isConnected: boolean;          // WebSocket open and receiving frames
  error: string | null;          // Last error message (null if OK)
  latencyMs: number | null;      // Time since last frame (shows stale >500ms)
}
```

## Binary Frame Format

**STATUS Frame (0xS)** — 20 bytes, sent from OpenRB → ESP every ~100ms

```
Byte  Type     Field         Value Range
────────────────────────────────────────────────────────────────
0     u8       Frame Type    0x53 (0xS)
1-2   u16 LE   Frequency     0–1000 (Hz × 10, so 500 = 50 Hz)
3-4   u16 LE   Position[0]   0–10000 (mm × 10)
5-6   u16 LE   Position[1]   0–10000 (mm × 10)
7-8   u16 LE   Position[2]   0–10000 (mm × 10)
9-10  u16 LE   Position[3]   0–10000 (mm × 10)
11-12 u16 LE   Force[0]      0–50000 (mV, raw)
13-14 u16 LE   Force[1]      0–50000 (mV, raw)
15-16 u16 LE   Force[2]      0–50000 (mV, raw)
17-18 u16 LE   Force[3]      0–50000 (mV, raw)
19    u8       CRC8          0–255 (checksum)
────────────────────────────────────────────────────────────────
Total: 20 bytes
```

### Example Frame

```
Hex: 53 F4 01 69 00 70 00 62 00 65 00 FC 08 A0 09 B8 08 AC 09 XX

Parsed:
  Type: 0x53 (0xS) ✓
  Frequency: 0x01F4 (500 LE) = 50 Hz
  Positions: [0x0069, 0x0070, 0x0062, 0x0065] LE = [10.5, 11.2, 9.8, 10.1] mm
  Forces: [0x08FC, 0x09A0, 0x08B8, 0x09AC] LE = [2300, 2400, 2200, 2300] mV
  CRC8: 0xXX (valid if matches crc8(bytes 0-18))
```

## Integration Points

### 1. **App.tsx** (Main component)

- Imports `useLiveStatus` hook
- Instantiates: `const liveStatus = useLiveStatus(espIp);`
- Merges live data into machine state:
  ```typescript
  // Use WebSocket data if available, else fall back to polled data
  frequency_hz: liveStatus.frequency ?? machineState.frequency_hz,
  positions: liveStatus.positions ?? machineState.positions,
  sensors: liveStatus.forces ?? machineState.sensors,
  ```
- Shows <50ms latency when WebSocket active

### 2. **StatusPanelSimple.tsx** (Status display)

- Receives merged machine state (with live data)
- Latency badge shows WebSocket quality:
  - Green: <150ms (good)
  - Amber: 150–400ms (acceptable)
  - Red: >400ms or "no link" (poor)

### 3. **useMachineController.ts** (HTTP polling fallback)

- Keeps `getStatus()` polling at 200ms (fallback)
- If WebSocket active and healthy, this becomes secondary
- On WebSocket error, HTTP polling takes over for data continuity

## Fallback Behavior

If WebSocket connection fails:

1. Hook logs error, sets `isConnected = false`
2. Component notices `liveStatus.isConnected = false`
3. Component falls back to HTTP polling data (from `machineState`)
4. Hook auto-reconnects every 1s with exponential backoff (max 30s)
5. On successful reconnect, live data takes over again

## Error Handling

| Error | Action |
|-------|--------|
| WebSocket connection fails | Log, set `isConnected=false`, fallback to HTTP |
| Frame CRC8 mismatch | Log warning, skip frame, continue listening |
| Malformed frame (wrong length, type) | Log warning, skip frame, continue listening |
| WebSocket closes | Attempt reconnect with exponential backoff |
| No frames for >500ms | Show `latencyMs` (indicates stale connection) |

## Performance Target

| Metric | HTTP Polling | WebSocket |
|--------|--------------|-----------|
| Latency | ~200ms | <50ms (target) |
| Frame size | 50–100 bytes (JSON) | 20 bytes (binary) |
| Throughput | 5 frames/sec | 10 frames/sec |
| CPU usage | Poll interval + parsing | Push on arrival |
| Memory | Frequent allocs (JSON) | Fixed (binary buffer) |

## Configuration

### ESP IP Discovery

Currently hardcoded to `'192.168.4.1'` in App.tsx:

```typescript
const [espIp, setEspIp] = useState<string>('192.168.4.1');
```

To enable dynamic discovery:

1. Add `GET /api/esp-config` endpoint to backend (returns `{esp_ip, esp_port}`)
2. Call on connection:
   ```typescript
   const config = await fetch(`${API_BASE}/esp-config`).then(r => r.json());
   setEspIp(config.esp_ip);
   ```

### Reconnection Strategy

- Initial delay: 1000ms + random jitter (0–1000ms)
- Backoff: delay *= 2 on each retry
- Max delay: 30 seconds
- Example: 1s → 2s → 4s → 8s → 16s → 30s → 30s → ...

## Testing

### Unit Tests (Binary Parsing)

See `src/app/hooks/__tests__/useLiveStatus.test.ts` for:
- CRC8 validation
- Frame parsing (all fields)
- Error handling (corrupted frames, wrong types, length)
- Edge cases (zero values, max values, fractions)

All tests pass ✅

### Integration Tests (TODO)

```typescript
// Mount hook, wait for connection
const { result, waitForNextUpdate } = renderHook(() =>
  useLiveStatus('192.168.4.1')
);

// Should connect
expect(result.current.isConnected).toBe(true);

// Mock WebSocket message with STATUS frame
const frame = buildStatusFrame(50, [10.5, 11.2, 9.8, 10.1], [2300, 2400, 2200, 2300]);
ws.emit('message', { data: frame.buffer });

// Should parse and update state
await waitForNextUpdate();
expect(result.current.frequency).toBe(50);
expect(result.current.positions).toEqual([10.5, 11.2, 9.8, 10.1]);
expect(result.current.forces).toEqual([2300, 2400, 2200, 2300]);
```

## Known Limitations

1. **ESP IP hardcoded** — Add discovery endpoint to backend
2. **No frame buffering** — If partial frame arrives, it's lost (rare, 20 bytes)
3. **No heartbeat** — Can't detect zombie connections; relies on 500ms timeout
4. **No authentication** — WebSocket is open (OK for local network)

## Future Enhancements

1. **Heartbeat/PING** — Add keep-alive to detect stale connections faster
2. **Frame compression** — Send delta-encoded positions if bandwidth is limited
3. **Fragmentation** — Support multi-frame responses (>20 bytes)
4. **Priority queue** — STOP commands always processed first (on backend)
5. **Metrics** — Track frame loss, latency percentiles, CRC errors

## Files Changed

| File | Changes |
|------|---------|
| `/app/hooks/useLiveStatus.ts` | **NEW** — WebSocket hook + binary parsing |
| `/app/hooks/__tests__/useLiveStatus.test.ts` | **NEW** — Binary parsing unit tests |
| `/app/App.tsx` | Import hook, integrate with components, fallback to polling |
| `/app/components/StatusPanelSimple.tsx` | Uses merged state (live + polled) |
| `PHASE_E_WEBSOCKET.md` | **NEW** — This file |

## References

- **Binary Protocol:** `/PROD/docs/17_BINARY_PROTOCOL.md`
- **Hook:** `/PROD/frontend/src/app/hooks/useLiveStatus.ts`
- **Integration:** `/PROD/frontend/src/app/App.tsx` (lines 33, 73–76, 229–242, 265, 293–294)
