"""
WiFiLink: WebSocket-based binary protocol gateway to ESP8266 (Phase 3)

Direct binary frame streaming via WebSocket:
- Command: [0x43][CMD_ID][ARGS...][CRC8] → ESP → OpenRB
- Status: [0x53][FREQ:2][POS:8][FORCE:8][CRC8] ← OpenRB ← ESP (streaming)
- <100ms latency, 3–20 byte frames

Protocol spec: PROD/docs/17_BINARY_PROTOCOL.md
"""

import struct
import logging
import threading
import queue
import time
from typing import Optional, Dict, Any

try:
    import websocket
except ImportError:
    websocket = None

logger = logging.getLogger(__name__)


def crc8(data: bytes) -> int:
    """CRC8 checksum (matches OpenRB firmware)."""
    crc = 0xFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def build_command_frame(cmd_id: int, args: bytes = b'') -> bytes:
    """Build binary command frame: [0x43][CMD_ID][ARGS...][CRC8]"""
    frame = bytes([0x43, cmd_id]) + args  # 0x43 = 'C'
    checksum = crc8(frame)
    return frame + bytes([checksum])


class WiFiLink:
    """WebSocket binary gateway to ESP8266."""

    def __init__(self, ip: str, http_port: int = 8080, auth_token: str = "", timeout: float = 2.0):
        self.ip = ip
        self.http_port = http_port
        self.auth_token = auth_token
        self.timeout = timeout
        self.ws_url = f"ws://{ip}:8080/ws"
        self.connected = False
        self.ws = None
        self.rx_queue = queue.Queue()
        self.rx_thread = None
        self.stop_event = threading.Event()

    def connect(self) -> bool:
        """Connect to ESP8266 via WebSocket."""
        if websocket is None:
            logger.error("websocket library not installed (pip install websocket-client)")
            return False

        print(f"[WiFiLink] Connecting to WebSocket: {self.ws_url}")

        try:
            self.ws = websocket.WebSocketApp(
                self.ws_url,
                on_message=self._on_message,
                on_error=self._on_error,
                on_close=self._on_close,
            )
            self.ws.on_open = self._on_open

            # Start WebSocket in background thread
            self.stop_event.clear()
            self.rx_thread = threading.Thread(target=self.ws.run_forever, daemon=True)
            self.rx_thread.start()

            # Wait for connection
            for _ in range(50):  # 5 second timeout
                if self.connected:
                    print(f"[WiFiLink] ✅ WebSocket connected")
                    return True
                time.sleep(0.1)

            print(f"[WiFiLink] ❌ WebSocket timeout")
            return False

        except Exception as e:
            print(f"[WiFiLink] ❌ Connection failed: {e}")
            logger.error(f"WebSocket connection failed: {e}")
            self.connected = False
            return False

    def disconnect(self) -> bool:
        """Disconnect from ESP8266."""
        self.connected = False
        self.stop_event.set()
        if self.ws:
            self.ws.close()
        if self.rx_thread:
            self.rx_thread.join(timeout=1.0)
        print(f"[WiFiLink] Disconnected")
        return True

    def _on_open(self, ws):
        """WebSocket connected."""
        self.connected = True

    def _on_message(self, ws, message):
        """Receive binary frame from WebSocket."""
        try:
            frame = bytes.fromhex(message) if isinstance(message, str) else message
            self.rx_queue.put(frame)
            print(f"[WiFiLink] RX: {frame.hex()}")
        except Exception as e:
            logger.error(f"Frame decode error: {e}")

    def _on_error(self, ws, error):
        """WebSocket error."""
        print(f"[WiFiLink] ❌ WebSocket error: {error}")
        logger.error(f"WebSocket error: {error}")
        self.connected = False

    def _on_close(self, ws, close_status_code, close_msg):
        """WebSocket closed."""
        print(f"[WiFiLink] WebSocket closed")
        self.connected = False

    def send_command(self, command: str, **kwargs) -> bool:
        """Send command via binary frame."""
        if not self.connected:
            logger.error("Not connected")
            return False

        cmd_upper = command.upper()
        frame = None

        try:
            if cmd_upper == 'START':
                frame = build_command_frame(0x01, b'')
            elif cmd_upper == 'STOP':
                frame = build_command_frame(0x02, b'')
            elif cmd_upper in ('HARD_RESET', 'HOME'):
                frame = build_command_frame(0x03, b'')
            elif cmd_upper == 'FREQUENCY':
                freq = int(kwargs.get('frequency', 0))
                freq = max(0, min(100, freq))
                frame = build_command_frame(0x10, bytes([freq]))
            elif cmd_upper == 'FORCE':
                sensor = kwargs.get('sensor')
                force_mv = int(kwargs.get('force', 0))
                force_mv = max(0, min(50000, force_mv))
                if sensor is not None:
                    sensor = max(1, min(4, int(sensor)))
                    frame = build_command_frame(0x11, bytes([sensor]) + struct.pack('<H', force_mv))
                else:
                    frame = build_command_frame(0x12, struct.pack('<H', force_mv))
            elif cmd_upper == 'GOTO':
                table = max(1, min(4, int(kwargs.get('table', 1))))
                pos_mm10 = int(kwargs.get('position', 0) * 10)
                pos_mm10 = max(0, min(10000, pos_mm10))
                frame = build_command_frame(0x20, bytes([table]) + struct.pack('<H', pos_mm10))
            elif cmd_upper == 'MOTOR_BLINK':
                motor = int(kwargs.get('motor_id', 0))
                duration = int(kwargs.get('duration_ms', 500))
                frame = build_command_frame(0x30, bytes([motor]) + struct.pack('<H', duration))
            elif cmd_upper == 'SCAN_DXL':
                frame = build_command_frame(0x31, b'')
            elif cmd_upper == 'SET_RESISTANCE':
                ohm = int(kwargs.get('resistance_ohm', 30))
                ohm = max(0, min(65535, ohm))
                frame = build_command_frame(0x40, struct.pack('<H', ohm))
            elif cmd_upper in ('STATUS', 'GET_STATUS'):
                frame = build_command_frame(0xF0, b'')
            else:
                print(f"[WiFiLink] ❌ Unknown command: {cmd_upper}")
                return False

            if frame:
                hex_str = frame.hex()
                print(f"[WiFiLink] TX: {hex_str}")
                self.ws.send(hex_str, websocket.ABNF.OPCODE_TEXT)
                return True

        except Exception as e:
            print(f"[WiFiLink] ❌ Send error: {e}")
            logger.error(f"Send error: {e}")
            return False

        return False

    def send_line(self, command_str: str) -> bool:
        """Compatibility with SerialLink interface."""
        command_str = command_str.strip().upper()

        if ':' in command_str:
            key, _, value = command_str.partition(':')
            key = key.strip()
            value = value.strip()

            if key == 'SET_FREQ' or key == 'SET_FREQUENCY':
                return self.send_command('FREQUENCY', frequency=float(value))
            elif key == 'SET_FORCE':
                parts = value.split(':')
                if len(parts) == 2:
                    sensor = int(parts[0])
                    force = float(parts[1])
                    return self.send_command('FORCE', force=force, sensor=sensor)
                else:
                    return self.send_command('FORCE', force=float(value))
            elif key == 'GOTO':
                parts = value.split(':')
                if len(parts) == 2:
                    return self.send_command('GOTO', table=int(parts[0]), position=float(parts[1]))

        if command_str in ('START', 'STOP', 'HOME', 'HARD_RESET', 'STATUS'):
            return self.send_command(command_str)

        return False

    def get_status(self) -> Optional[Dict[str, Any]]:
        """Get cached status (WebSocket streaming provides live data)."""
        # Try to get one frame from queue without blocking
        try:
            frame = self.rx_queue.get_nowait()
            if len(frame) == 20 and frame[0] == 0x53:  # STATUS frame
                freq_hz10 = frame[1] | (frame[2] << 8)
                positions = []
                for i in range(4):
                    pos_mm10 = frame[3 + i*2] | (frame[4 + i*2] << 8)
                    positions.append(pos_mm10 / 10.0)
                forces = []
                for i in range(4):
                    force_mv = frame[11 + i*2] | (frame[12 + i*2] << 8)
                    forces.append(force_mv / 1000.0)

                return {
                    'frequency': freq_hz10 / 10.0,
                    'positions': positions,
                    'sensors': forces,
                    'status': 'ok',
                }
        except queue.Empty:
            pass

        return None

    def is_open(self) -> bool:
        return self.connected
