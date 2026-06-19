/**
 * Test file for useLiveStatus hook binary parsing logic.
 * This tests the core parsing function without needing a real WebSocket.
 * 
 * Run with Jest:
 *   npm test -- useLiveStatus.test.ts
 * 
 * Or manually verify with test_parsing.js reference implementation.
 */

/**
 * Binary parsing unit tests — reference implementation
 * Tests verify the CRC8, frame type, length validation and data extraction
 */

// Reference implementation (duplicated for testing without imports)
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

interface StatusFrameData {
  frequency: number;
  positions: number[];
  forces: number[];
  timestamp: number;
}

function parseStatusFrame(data: Uint8Array): StatusFrameData {
  if (data.length !== 20) {
    throw new Error(`Invalid frame length: expected 20, got ${data.length}`);
  }

  if (data[0] !== 0x53) {
    throw new Error(`Invalid frame type: expected 0x53, got 0x${data[0].toString(16)}`);
  }

  const frameChecksum = data[19];
  const calculatedChecksum = crc8(data.subarray(0, 19));
  if (frameChecksum !== calculatedChecksum) {
    throw new Error(`CRC8 mismatch: expected 0x${frameChecksum.toString(16)}, got 0x${calculatedChecksum.toString(16)}`);
  }

  const dv = new DataView(data.buffer, data.byteOffset, data.byteLength);
  const freq_hz10 = dv.getUint16(1, true);
  const frequency = freq_hz10 / 10.0;

  const positions: number[] = [];
  for (let i = 0; i < 4; i++) {
    const pos_mm10 = dv.getUint16(3 + i * 2, true);
    positions.push(pos_mm10 / 10.0);
  }

  const forces: number[] = [];
  for (let i = 0; i < 4; i++) {
    const force_mv = dv.getUint16(11 + i * 2, true);
    forces.push(force_mv);
  }

  return {
    frequency,
    positions,
    forces,
    timestamp: performance.now(),
  };
}

function buildFrame(
  frequency_hz: number,
  positions: number[],
  forces: number[]
): Uint8Array {
  const frame = new Uint8Array(20);
  const dv = new DataView(frame.buffer);

  frame[0] = 0x53;
  dv.setUint16(1, Math.round(frequency_hz * 10), true);

  for (let i = 0; i < 4; i++) {
    dv.setUint16(3 + i * 2, Math.round(positions[i] * 10), true);
  }

  for (let i = 0; i < 4; i++) {
    dv.setUint16(11 + i * 2, Math.round(forces[i]), true);
  }

  frame[19] = crc8(frame.subarray(0, 19));

  return frame;
}

describe('useLiveStatus - Binary Frame Parsing', () => {
  it('parses valid frame from 17_BINARY_PROTOCOL.md spec', () => {
    const frame = buildFrame(50, [10.5, 11.2, 9.8, 10.1], [2300, 2400, 2200, 2300]);
    const parsed = parseStatusFrame(frame);

    expect(parsed.frequency).toBe(50);
    expect(parsed.positions).toEqual([10.5, 11.2, 9.8, 10.1]);
    expect(parsed.forces).toEqual([2300, 2400, 2200, 2300]);
  });

  it('parses zero values correctly', () => {
    const frame = buildFrame(0, [0, 0, 0, 0], [0, 0, 0, 0]);
    const parsed = parseStatusFrame(frame);

    expect(parsed.frequency).toBe(0);
    expect(parsed.positions).toEqual([0, 0, 0, 0]);
    expect(parsed.forces).toEqual([0, 0, 0, 0]);
  });

  it('parses maximum values correctly', () => {
    const frame = buildFrame(100, [1000, 1000, 1000, 1000], [50000, 50000, 50000, 50000]);
    const parsed = parseStatusFrame(frame);

    expect(parsed.frequency).toBe(100);
    expect(parsed.positions).toEqual([1000, 1000, 1000, 1000]);
    expect(parsed.forces).toEqual([50000, 50000, 50000, 50000]);
  });

  it('rejects frame with invalid CRC8', () => {
    const frame = buildFrame(50, [10.5, 11.2, 9.8, 10.1], [2300, 2400, 2200, 2300]);
    frame[5] = (frame[5] + 1) & 0xFF;  // Corrupt payload

    expect(() => parseStatusFrame(frame)).toThrow(/CRC8 mismatch/);
  });

  it('rejects frame with wrong type', () => {
    const frame = buildFrame(50, [10.5, 11.2, 9.8, 10.1], [2300, 2400, 2200, 2300]);
    frame[0] = 0x52;  // 0xR instead of 0xS

    expect(() => parseStatusFrame(frame)).toThrow(/Invalid frame type/);
  });

  it('rejects frame with wrong length', () => {
    const shortFrame = new Uint8Array(19);

    expect(() => parseStatusFrame(shortFrame)).toThrow(/Invalid frame length/);
  });

  it('parses fractional values within rounding tolerance', () => {
    const frame = buildFrame(33.7, [12.34, 56.78, 90.12, 34.56], [1234, 5678, 9012, 3456]);
    const parsed = parseStatusFrame(frame);

    expect(Math.abs(parsed.frequency - 33.7)).toBeLessThan(0.1);
    expect(parsed.positions.every((p, i) => Math.abs(p - [12.34, 56.78, 90.12, 34.56][i]) < 0.15)).toBe(true);
    expect(parsed.forces).toEqual([1234, 5678, 9012, 3456]);
  });

  it('includes timestamp in parsed frame', () => {
    const frame = buildFrame(50, [10.5, 11.2, 9.8, 10.1], [2300, 2400, 2200, 2300]);
    const before = performance.now();
    const parsed = parseStatusFrame(frame);
    const after = performance.now();

    expect(parsed.timestamp).toBeGreaterThanOrEqual(before);
    expect(parsed.timestamp).toBeLessThanOrEqual(after + 1);  // Allow 1ms tolerance
  });
});
