/**
 * ===========================================================================
 * FILE: useLiveStatus.ts
 * ROLE:
 *   Real-time WebSocket hook for binary status frame streaming.
 *   Replaces HTTP polling (200ms) with direct binary 0xS frames (<50ms latency).
 *
 * PROTOCOL:
 *   - Connects to ws://[esp_ip]:8080/ws
 *   - Listens for STATUS frames (0xS) from binary protocol (17_BINARY_PROTOCOL.md)
 *   - Parses fixed 20-byte binary frame: [0xS][FREQ:2][POS:8][FORCE:8][CRC8:1]
 *
 * STATE:
 *   {frequency, positions, forces, isConnected, error}
 *
 * FALLBACK:
 *   - If WebSocket fails to connect/sustains error, returns false for isConnected
 *   - Caller (Component or useMachineController) should fallback to HTTP polling
 *
 * AUTO-RECONNECT:
 *   - On disconnect: waits 1s + jitter, retries with exponential backoff (max 30s)
 *   - On malformed frame: logs warning, skips, continues listening
 *
 * UNMOUNT:
 *   - Closes WebSocket + clears intervals automatically
 * ===========================================================================
 */

import { useState, useEffect, useRef, useCallback } from 'react';

export interface StatusFrameData {
  frequency: number;      // Hz (decoded from u16 LE / 10)
  positions: number[];    // mm (4 values, decoded from u16 LE / 10 each)
  forces: number[];       // mV (4 values, decoded from u16 LE)
  timestamp: number;      // milliseconds when frame arrived
}

export interface UseLiveStatusReturn {
  frequency: number | null;
  positions: number[] | null;
  forces: number[] | null;
  isConnected: boolean;
  error: string | null;
  latencyMs: number | null;  // time since last frame arrived
}

/**
 * CRC8 checksum (polynomial 0x07, initial value 0xFF).
 * Used to validate STATUS frames (last byte is checksum of bytes 0-18).
 */
function crc8(data: Uint8Array): number {
  let crc = 0xFF;
  for (let i = 0; i < data.length; i++) {
    crc ^= data[i];
    for (let j = 0; j < 8; j++) {
      if (crc & 0x80) {
        crc = ((crc << 1) ^ 0x07) & 0xFF;
      } else {
        crc = (crc << 1) & 0xFF;
      }
    }
  }
  return crc;
}

/**
 * Parse a single STATUS frame (0xS).
 * Frame format (20 bytes total):
 *   [0]: 0xS (0x53)
 *   [1-2]: frequency (u16 LE) = Hz × 10
 *   [3-4]: position[0] (u16 LE) = mm × 10
 *   [5-6]: position[1] (u16 LE) = mm × 10
 *   [7-8]: position[2] (u16 LE) = mm × 10
 *   [9-10]: position[3] (u16 LE) = mm × 10
 *   [11-12]: force[0] (u16 LE) = mV
 *   [13-14]: force[1] (u16 LE) = mV
 *   [15-16]: force[2] (u16 LE) = mV
 *   [17-18]: force[3] (u16 LE) = mV
 *   [19]: CRC8
 *
 * Throws on validation error (malformed, bad length, CRC mismatch).
 */
function parseStatusFrame(data: Uint8Array): StatusFrameData {
  if (data.length !== 20) {
    throw new Error(`Invalid frame length: expected 20, got ${data.length}`);
  }

  // Validate frame type
  if (data[0] !== 0x53) {  // 0xS = 0x53
    throw new Error(`Invalid frame type: expected 0x53 (0xS), got 0x${data[0].toString(16).padStart(2, '0')}`);
  }

  // Validate CRC8 (checksum of bytes 0-18, stored at byte 19)
  const frameChecksum = data[19];
  const calculatedChecksum = crc8(data.subarray(0, 19));
  if (frameChecksum !== calculatedChecksum) {
    throw new Error(
      `CRC8 mismatch: expected 0x${frameChecksum.toString(16).padStart(2, '0')}, ` +
      `got 0x${calculatedChecksum.toString(16).padStart(2, '0')}`
    );
  }

  // Parse using DataView for little-endian u16 reads
  const dv = new DataView(data.buffer, data.byteOffset, data.byteLength);

  // Frequency: u16 LE at offset 1, value is Hz × 10, divide by 10 to get Hz
  const freq_hz10 = dv.getUint16(1, true);  // little-endian
  const frequency = freq_hz10 / 10.0;

  // Positions: 4 × u16 LE at offsets 3, 5, 7, 9 (each mm × 10)
  const positions: number[] = [];
  for (let i = 0; i < 4; i++) {
    const pos_mm10 = dv.getUint16(3 + i * 2, true);  // little-endian
    positions.push(pos_mm10 / 10.0);
  }

  // Forces: 4 × u16 LE at offsets 11, 13, 15, 17 (each mV)
  const forces: number[] = [];
  for (let i = 0; i < 4; i++) {
    const force_mv = dv.getUint16(11 + i * 2, true);  // little-endian
    forces.push(force_mv);
  }

  return {
    frequency,
    positions,
    forces,
    timestamp: performance.now(),
  };
}

/**
 * React hook: connect to ESP WebSocket and stream binary STATUS frames.
 * @param esp_ip - IP address of ESP device (e.g., "192.168.4.1")
 * @returns {frequency, positions, forces, isConnected, error, latencyMs}
 *
 * Usage:
 *   const status = useLiveStatus('192.168.4.1');
 *   console.log('Frequency:', status.frequency, 'Hz');
 *   console.log('Position 0:', status.positions?.[0], 'mm');
 *   console.log('Force 0:', status.forces?.[0], 'mV');
 *   console.log('Connected:', status.isConnected);
 */
export function useLiveStatus(esp_ip: string): UseLiveStatusReturn {
  const [frequency, setFrequency] = useState<number | null>(null);
  const [positions, setPositions] = useState<number[] | null>(null);
  const [forces, setForces] = useState<number[] | null>(null);
  const [isConnected, setIsConnected] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [latencyMs, setLatencyMs] = useState<number | null>(null);

  // Refs for WebSocket and reconnection logic
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimeoutRef = useRef<NodeJS.Timeout | null>(null);
  const reconnectDelayRef = useRef(1000);  // Start at 1s, backoff exponentially
  const lastFrameTimeRef = useRef<number>(performance.now());
  const latencyIntervalRef = useRef<NodeJS.Timeout | null>(null);

  // Cleanup function: close WebSocket and clear intervals
  const cleanup = useCallback(() => {
    if (reconnectTimeoutRef.current) {
      clearTimeout(reconnectTimeoutRef.current);
      reconnectTimeoutRef.current = null;
    }
    if (latencyIntervalRef.current) {
      clearInterval(latencyIntervalRef.current);
      latencyIntervalRef.current = null;
    }
    if (wsRef.current) {
      wsRef.current.close();
      wsRef.current = null;
    }
    setIsConnected(false);
  }, []);

  // Attempt to connect to WebSocket
  const connect = useCallback(() => {
    if (wsRef.current?.readyState === WebSocket.OPEN || wsRef.current?.readyState === WebSocket.CONNECTING) {
      return;  // Already connecting or connected
    }

    try {
      console.log(`[useLiveStatus] Connecting to ws://${esp_ip}:8080/ws`);
      const ws = new WebSocket(`ws://${esp_ip}:8080/ws`);
      ws.binaryType = 'arraybuffer';  // Receive as binary

      ws.onopen = () => {
        console.log('[useLiveStatus] WebSocket connected');
        setIsConnected(true);
        setError(null);
        reconnectDelayRef.current = 1000;  // Reset backoff on successful connect
        lastFrameTimeRef.current = performance.now();
      };

      ws.onmessage = (event) => {
        if (typeof event.data !== 'string') {
          // Binary frame
          const data = new Uint8Array(event.data);

          // Filter: Only process STATUS frames (0x53 = 'S')
          // Ignore ACK/DONE frames (0x52 = 'R') and other frame types
          if (data.length > 0 && data[0] === 0x53) {
            try {
              const parsed = parseStatusFrame(data);
              setFrequency(parsed.frequency);
              setPositions(parsed.positions);
              setForces(parsed.forces);
              lastFrameTimeRef.current = performance.now();
            } catch (err) {
              // Log but don't crash; just skip this frame and wait for next one
              const msg = err instanceof Error ? err.message : String(err);
              console.warn(`[useLiveStatus] Failed to parse STATUS frame: ${msg}`);
              // Optionally log hex for debugging
              if (data.length > 0) {
                const hex = Array.from(data.slice(0, 20))
                  .map((b) => b.toString(16).padStart(2, '0'))
                  .join(' ');
                console.warn(`[useLiveStatus] Frame hex (first 20): ${hex}`);
              }
            }
          }
          // Silently ignore non-STATUS frames (ACK, DONE, etc.)
        }
      };

      ws.onerror = (event) => {
        const errMsg = `WebSocket error: ${event.type}`;
        console.error(`[useLiveStatus] ${errMsg}`);
        setError(errMsg);
        setIsConnected(false);
      };

      ws.onclose = () => {
        console.log('[useLiveStatus] WebSocket closed, scheduling reconnect');
        setIsConnected(false);
        wsRef.current = null;

        // Schedule reconnection with exponential backoff (max 30s)
        const delay = Math.min(reconnectDelayRef.current + Math.random() * 1000, 30000);
        reconnectTimeoutRef.current = setTimeout(() => {
          reconnectDelayRef.current = Math.min(reconnectDelayRef.current * 2, 30000);
          connect();
        }, delay);
      };

      wsRef.current = ws;
    } catch (err) {
      const errMsg = err instanceof Error ? err.message : String(err);
      console.error(`[useLiveStatus] Failed to create WebSocket: ${errMsg}`);
      setError(errMsg);
      setIsConnected(false);

      // Retry after delay
      const delay = Math.min(reconnectDelayRef.current + Math.random() * 1000, 30000);
      reconnectTimeoutRef.current = setTimeout(() => {
        reconnectDelayRef.current = Math.min(reconnectDelayRef.current * 2, 30000);
        connect();
      }, delay);
    }
  }, [esp_ip]);

  // Update latency: elapsed time since last frame
  useEffect(() => {
    latencyIntervalRef.current = setInterval(() => {
      if (isConnected) {
        const elapsed = Math.round(performance.now() - lastFrameTimeRef.current);
        setLatencyMs(elapsed > 500 ? elapsed : null);  // Only show if > 500ms (indicates stale)
      } else {
        setLatencyMs(null);
      }
    }, 100);

    return () => {
      if (latencyIntervalRef.current) clearInterval(latencyIntervalRef.current);
    };
  }, [isConnected]);

  // Connect on mount, cleanup on unmount
  useEffect(() => {
    connect();

    return () => {
      cleanup();
    };
  }, [connect, cleanup, esp_ip]);

  return {
    frequency,
    positions,
    forces,
    isConnected,
    error,
    latencyMs,
  };
}
