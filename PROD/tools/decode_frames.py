#!/usr/bin/env python3
"""
Binary Protocol Frame Decoder

Usage:
    python decode_frames.py --port /dev/ttyUSB0 --baud 19200 [--verbose]

Reads raw binary frames from serial port and pretty-prints them with timestamps,
colors, and human-readable interpretation.

Frame types:
    0xC — Command (host → device)
    0xR — Response (device → host)
    0xS — Status stream (device → host, continuous)

Features:
    - Real-time parsing with frame boundary detection
    - Color-coded output (status=blue, ACK=green, ERROR=red)
    - CRC8 validation
    - CSV export (--csv out.csv)
"""

import serial
import argparse
import sys
import time
from datetime import datetime
from typing import Optional, Tuple, List


# ANSI color codes
class Colors:
    RESET = "\033[0m"
    BLUE = "\033[94m"
    GREEN = "\033[92m"
    RED = "\033[91m"
    YELLOW = "\033[93m"
    GRAY = "\033[90m"


# Command IDs (from protocol spec)
COMMANDS = {
    0x01: "START",
    0x02: "STOP (soft hold)",
    0x03: "HARD_RESET",
    0x04: "HOME (alias HARD_RESET)",
    0x10: "SET_FREQ",
    0x11: "SET_FORCE",
    0x12: "SET_FORCE_ALL",
    0x20: "GOTO",
    0x30: "MOTOR_BLINK",
    0x31: "SCAN_DXL",
    0x40: "SET_RESISTANCE",
    0xF0: "GET_STATUS",
}

# Response codes (from protocol spec)
RESPONSES = {
    0x00: "ACK",
    0x01: "DONE",
    0x80: "ERROR_INVALID_CMD",
    0x81: "ERROR_INVALID_ARG",
    0x82: "ERROR_CRC",
    0x83: "ERROR_DEVICE",
}


def crc8(data: bytes) -> int:
    """Compute CRC8 checksum (matches OpenRB firmware)."""
    crc = 0xFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def validate_checksum(frame: bytes) -> bool:
    """Check if frame checksum is valid."""
    if len(frame) < 2:
        return False
    payload = frame[:-1]
    expected_crc = frame[-1]
    computed_crc = crc8(payload)
    return computed_crc == expected_crc


def parse_command_frame(data: bytes) -> str:
    """Parse command frame (0xC)."""
    if len(data) < 2:
        return "INCOMPLETE"

    cmd_id = data[1]
    cmd_name = COMMANDS.get(cmd_id, f"UNKNOWN(0x{cmd_id:02X})")

    # Parse arguments based on command
    args_str = ""

    if cmd_id == 0x10:  # SET_FREQ
        if len(data) >= 3:
            freq_hz = data[2]
            args_str = f" freq={freq_hz} Hz"

    elif cmd_id == 0x11:  # SET_FORCE
        if len(data) >= 5:
            sensor = data[2]
            target_mv = data[3] | (data[4] << 8)  # u16 LE
            target_n = target_mv / 1000.0
            args_str = f" sensor={sensor} target={target_n:.1f} N ({target_mv} mV)"

    elif cmd_id == 0x12:  # SET_FORCE_ALL
        if len(data) >= 4:
            target_mv = data[2] | (data[3] << 8)  # u16 LE
            target_n = target_mv / 1000.0
            args_str = f" target_all={target_n:.1f} N ({target_mv} mV)"

    elif cmd_id == 0x20:  # GOTO
        if len(data) >= 5:
            table = data[2]
            pos_mm10 = data[3] | (data[4] << 8)  # u16 LE
            pos_mm = pos_mm10 / 10.0
            args_str = f" table={table} pos={pos_mm:.1f} mm"

    elif cmd_id == 0x30:  # MOTOR_BLINK
        if len(data) >= 5:
            motor_id = data[2]
            duration_ms = data[3] | (data[4] << 8)  # u16 LE
            args_str = f" motor_id={motor_id} duration={duration_ms} ms"

    elif cmd_id == 0x40:  # SET_RESISTANCE
        if len(data) >= 4:
            resistance = data[2] | (data[3] << 8)  # u16 LE
            args_str = f" resistance={resistance} Ω"

    return f"COMMAND {cmd_name}{args_str}"


def parse_response_frame(data: bytes) -> str:
    """Parse response frame (0xR)."""
    if len(data) < 2:
        return "INCOMPLETE"

    result_code = data[1]
    result_name = RESPONSES.get(result_code, f"UNKNOWN(0x{result_code:02X})")

    return f"RESPONSE {result_name}"


def parse_status_frame(data: bytes) -> str:
    """Parse status frame (0xS)."""
    if len(data) != 20:
        return f"STATUS (MALFORMED: expected 20 bytes, got {len(data)})"

    # Extract fields
    freq_hz10 = data[1] | (data[2] << 8)
    freq_hz = freq_hz10 / 10.0

    positions = []
    for i in range(4):
        pos_mm10 = data[3 + i*2] | (data[4 + i*2] << 8)
        pos_mm = pos_mm10 / 10.0
        positions.append(pos_mm)

    forces = []
    for i in range(4):
        force_mv = data[11 + i*2] | (data[12 + i*2] << 8)
        force_n = force_mv / 1000.0
        forces.append(force_n)

    pos_str = ", ".join(f"{p:.1f}" for p in positions)
    force_str = ", ".join(f"{f:.2f}" for f in forces)

    return f"STATUS freq={freq_hz:.1f} Hz | pos=[{pos_str}] mm | force=[{force_str}] N"


def decode_frame(data: bytes) -> Tuple[str, bool]:
    """
    Decode a binary frame.

    Returns: (description, is_valid)
    """
    if len(data) < 2:
        return f"FRAME TOO SHORT ({len(data)} bytes)", False

    frame_type = data[0]
    is_valid = validate_checksum(data)

    if frame_type == 0x43:  # 'C' = Command
        desc = parse_command_frame(data)
    elif frame_type == 0x52:  # 'R' = Response
        desc = parse_response_frame(data)
    elif frame_type == 0x53:  # 'S' in ASCII, but protocol uses 0xS as marker
        desc = parse_status_frame(data)
    else:
        desc = f"UNKNOWN FRAME TYPE (0x{frame_type:02X})"

    return desc, is_valid


class FrameDecoder:
    """Stateful frame decoder for streaming serial data."""

    def __init__(self, verbose: bool = False):
        self.buffer = bytearray()
        self.verbose = verbose
        self.frame_count = 0
        self.error_count = 0
        self.csv_rows: List[str] = []

    def feed(self, byte: int) -> Optional[Tuple[bytes, str, bool]]:
        """
        Feed one byte. Returns (frame_bytes, description, is_valid) if frame complete.
        """
        self.buffer.append(byte)

        # Frame boundary detection: look for frame type markers
        if len(self.buffer) >= 1:
            frame_type = self.buffer[0]

            # Detect frame length based on type
            expected_len = None

            if frame_type == 0x43:  # Command: 3–8 bytes
                # We'll assume frames complete when we see a complete command
                # For now, use heuristic: if buffer > 8 bytes and no valid CRC found,
                # try to find the next frame boundary
                if len(self.buffer) >= 8:
                    # Try to validate
                    for try_len in range(3, min(len(self.buffer) + 1, 9)):
                        frame = bytes(self.buffer[:try_len])
                        if validate_checksum(frame):
                            return self._extract_frame(try_len)

            elif frame_type == 0x52:  # Response: 3 bytes minimum
                if len(self.buffer) >= 3:
                    frame = bytes(self.buffer[:3])
                    if validate_checksum(frame):
                        return self._extract_frame(3)

            elif frame_type == 0x53:  # Status: 20 bytes
                if len(self.buffer) >= 20:
                    frame = bytes(self.buffer[:20])
                    if validate_checksum(frame):
                        return self._extract_frame(20)

            # Safety: if buffer gets too large, discard oldest byte
            if len(self.buffer) > 100:
                self.buffer.pop(0)

        return None

    def _extract_frame(self, length: int) -> Tuple[bytes, str, bool]:
        """Extract and decode a frame."""
        frame = bytes(self.buffer[:length])
        self.buffer = self.buffer[length:]

        desc, is_valid = decode_frame(frame)
        self.frame_count += 1
        if not is_valid:
            self.error_count += 1

        return frame, desc, is_valid

    def format_output(self, frame: bytes, desc: str, is_valid: bool) -> str:
        """Format output line with timestamp, colors, and hex dump."""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        hex_dump = " ".join(f"{b:02X}" for b in frame)

        if not is_valid:
            color = Colors.RED
            status = "❌"
        elif "ERROR" in desc:
            color = Colors.RED
            status = "⚠️"
        elif "ACK" in desc or "DONE" in desc:
            color = Colors.GREEN
            status = "✅"
        elif "STATUS" in desc:
            color = Colors.BLUE
            status = "📊"
        else:
            color = Colors.YELLOW
            status = "➜"

        return f"{color}[{timestamp}] {status} {desc}{Colors.RESET} | {Colors.GRAY}{hex_dump}{Colors.RESET}"


def main():
    parser = argparse.ArgumentParser(
        description="Binary Protocol Frame Decoder for OpenRB ↔ ESP communication"
    )
    parser.add_argument("--port", required=True, help="Serial port (e.g., /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=19200, help="Baud rate (default: 19200)")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument("--csv", help="Export frames to CSV file")

    args = parser.parse_args()

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
        print(f"✅ Connected to {args.port} @ {args.baud} baud")
        print("Press Ctrl+C to stop.\n")
    except Exception as e:
        print(f"❌ Failed to open port {args.port}: {e}")
        sys.exit(1)

    decoder = FrameDecoder(verbose=args.verbose)

    try:
        while True:
            if ser.in_waiting > 0:
                byte = ser.read(1)[0]
                result = decoder.feed(byte)

                if result:
                    frame, desc, is_valid = result
                    output = decoder.format_output(frame, desc, is_valid)
                    print(output)

                    if args.csv:
                        # Simple CSV: timestamp, valid, type, description
                        decoder.csv_rows.append(
                            f"{datetime.now().isoformat()},{is_valid},{frame[0]:02X},{desc}"
                        )
            else:
                time.sleep(0.001)  # Small sleep to prevent busy-waiting

    except KeyboardInterrupt:
        print(f"\n\n📈 Summary:")
        print(f"  Frames decoded: {decoder.frame_count}")
        print(f"  CRC errors: {decoder.error_count}")
        print(f"  Success rate: {(decoder.frame_count - decoder.error_count) / max(decoder.frame_count, 1) * 100:.1f}%")

        if args.csv:
            with open(args.csv, 'w') as f:
                f.write("timestamp,valid,type,description\n")
                for row in decoder.csv_rows:
                    f.write(row + "\n")
            print(f"  CSV export: {args.csv}")

        ser.close()
        print("\n✅ Decoder closed.")


if __name__ == "__main__":
    main()
