"""
===============================================================================
FILE: comm/wifi_link.py
ROLE:
    Lien de communication backend <-> ESP8266 via binary TCP frames
    (Phase D: haute performance, <100ms latency).

ARCHITECTURE:
    MachineController -> WiFiLink --TCP port 9000--> ESP8266 (192.168.4.1:9000) --série--> OpenRB-150
    WiFiLink --HTTP GET--> ESP8266:8080/api/status (phase C: STATUS frame cache JSON)

RESPONSIBILITIES:
    - send_frame(frame: bytes): envoie un frame binaire via TCP port 9000, attend
      réponse (ACK/ERROR), 100ms timeout, fire-and-forget.
    - send_command(): traduit commandes REST en frames binaires, appelle send_frame().
    - send_line(): garde compatibilité, parse colon-delimited (SET_FREQ:50) en frame.
    - get_status(): lit cache HTTP /api/status (ESP retourne STATUS frame en JSON).
    - Sérialise accès TCP via _http_lock (une frame à la fois).

DEPENDENCIES:
    - socket (TCP client), requests (GET /api/status), threading (lock), struct (u16 LE).

BINARY PROTOCOL:
    - Command: [0xC] [CMD_ID] [ARGS...] [CRC8]
    - Response: [0xR] [RESULT_CODE] [DATA] [CRC8]
    - CRC8: XOR initial 0xFF, polynomial 0x07

MAINTAINER NOTES:
    - Timeout 100ms pour TCP (court, mais local WiFi)
    - Pas de retry sur timeout : let caller handle (plus simple, déterministe)
    - get_status() reste HTTP (pas binaire : STATUS frame est trop volumineux pour du parsage ici)
    - read_line() deprecated (réponses ne sont pas bufferisées, sauf compatibilité)
===============================================================================
"""

import socket
import struct
import requests
import json
import logging
import threading
from typing import Optional, Dict, Any, Tuple

logger = logging.getLogger(__name__)


# ============================================================================
# CRC8 Checksum (Binary Protocol)
# ============================================================================

def crc8(data: bytes) -> int:
    """
    Calculate CRC8 checksum per 17_BINARY_PROTOCOL.md.

    Algorithm:
    - Initial: crc = 0xFF
    - For each byte: crc ^= byte, then shift 8 times with polynomial 0x07

    Args:
        data: bytes to checksum

    Returns:
        CRC8 byte (0-255)
    """
    crc = 0xFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = (crc << 1) ^ 0x07
            else:
                crc = (crc << 1)
            crc &= 0xFF
    return crc


def build_command_frame(cmd_id: int, args: bytes = b'') -> bytes:
    """
    Build a binary command frame with CRC8.

    Format: [0xC] [CMD_ID] [ARGS...] [CRC8]

    Args:
        cmd_id: Command ID (0x01-0xF0)
        args: Optional argument bytes (0-6 bytes)

    Returns:
        Complete frame including CRC8 checksum
    """
    frame_data = bytes([0xC, cmd_id]) + args
    checksum = crc8(frame_data)
    return frame_data + bytes([checksum])


def parse_response_frame(frame: bytes) -> Tuple[int, bytes]:
    """
    Parse a binary response frame.

    Format: [0xR] [RESULT_CODE] [DATA...] [CRC8]

    Args:
        frame: Raw response frame bytes

    Returns:
        Tuple of (result_code, data_bytes)
        Raises ValueError if frame invalid or CRC mismatch
    """
    if len(frame) < 3:
        raise ValueError(f"Response frame too short: {len(frame)} bytes")

    if frame[0] != 0xR:
        raise ValueError(f"Invalid response frame type: {frame[0]:02x} (expected 0x52)")

    result_code = frame[1]
    data = frame[2:-1] if len(frame) > 3 else b''
    checksum = frame[-1]

    # Verify CRC8
    frame_without_crc = frame[:-1]
    expected_crc = crc8(frame_without_crc)
    if expected_crc != checksum:
        raise ValueError(f"CRC mismatch: got {checksum:02x}, expected {expected_crc:02x}")

    return result_code, data


class WiFiLink:
    """Binary TCP + HTTP communication link to ESP8266 controller (Phase D)."""

    # TCP binary frame protocol
    TCP_PORT = 9000  # Raw binary TCP server port
    TCP_TIMEOUT = 0.1  # 100ms timeout per send_frame() call

    def __init__(self, ip: str, http_port: int = 8080, auth_token: str = "", timeout: float = 2.0):
        """
        Initialize WiFi link.

        Args:
            ip: ESP8266 IP address
            http_port: ESP8266 HTTP server port (for /api/status only)
            auth_token: Bearer token for authentication
            timeout: HTTP timeout in seconds (get_status)
        """
        self.ip = ip
        self.http_port = http_port
        self.auth_token = auth_token
        self.timeout = timeout
        self.base_url = f"http://{ip}:{http_port}"
        self.connected = False
        # Sérialisation des accès TCP+HTTP : une opération à la fois.
        # Évite les race conditions sur la connexion.
        self._http_lock = threading.Lock()
        # Session réutilisée (keep-alive) : pour GET /api/status seulement.
        self._session = requests.Session()

    @property
    def headers(self) -> Dict[str, str]:
        """Get headers with authentication."""
        return {
            "Content-Type": "application/json",
            "Authorization": f"Bearer {self.auth_token}"
        }

    def connect(self) -> bool:
        """Test connection to ESP8266 via TCP ping (send empty frame)."""
        print(f"[WiFiLink] Testing connection to: {self.ip}:{self.TCP_PORT}")

        try:
            # Try a simple ping: GET_STATUS (0xF0) frame
            frame = build_command_frame(0xF0, b'')  # GET_STATUS
            result = self.send_frame(frame)

            self.connected = result
            if self.connected:
                print(f"[WiFiLink] ✅ Connecté via TCP")
                logger.info(f"Connected to ESP8266 at {self.ip}:{self.TCP_PORT}")
            else:
                print(f"[WiFiLink] ❌ TCP frame timeout or error")
                logger.error(f"ESP8266 TCP connection failed")
            return self.connected

        except Exception as e:
            print(f"[WiFiLink] ❌ Connection test failed: {e}")
            logger.error(f"Failed to connect to ESP8266: {e}")
            self.connected = False
            return False

    def disconnect(self) -> bool:
        """Disconnect from ESP8266."""
        self.connected = False
        logger.info("Disconnected from ESP8266")
        return True

    def send_frame(self, frame: bytes) -> bool:
        """
        Send binary frame via raw TCP to ESP port 9000.

        Protocol:
        - Open TCP socket to ESP:9000
        - Send frame bytes directly
        - Wait for response frame (ACK/ERROR), 100ms timeout
        - Close socket
        - Return True if ACK (result code 0x00), False on timeout/error

        Args:
            frame: Binary frame to send (should include CRC8)

        Returns:
            True if response is ACK (0x00), False if timeout/error/NACK
        """
        if not self.connected:
            logger.error("Not connected to ESP8266")
            return False

        with self._http_lock:
            sock = None
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(self.TCP_TIMEOUT)

                # Send frame
                print(f"[WiFiLink] TCP send: {frame.hex()}")
                sock.connect((self.ip, self.TCP_PORT))
                sock.sendall(frame)

                # Wait for response
                response = sock.recv(1024)
                print(f"[WiFiLink] TCP recv: {response.hex()}")

                if not response:
                    print(f"[WiFiLink] ❌ No response from ESP")
                    return False

                # Parse response frame
                try:
                    result_code, data = parse_response_frame(response)
                    is_ack = (result_code == 0x00)  # ACK result code
                    if is_ack:
                        print(f"[WiFiLink] ✅ ACK received")
                    else:
                        print(f"[WiFiLink] ❌ Response code {result_code:02x}")
                    return is_ack

                except ValueError as e:
                    print(f"[WiFiLink] ❌ Invalid response frame: {e}")
                    logger.error(f"Invalid response frame: {e}")
                    return False

            except socket.timeout:
                print(f"[WiFiLink] ⚠️ TCP timeout ({self.TCP_TIMEOUT}s)")
                logger.warning(f"TCP timeout waiting for response")
                return False

            except Exception as e:
                print(f"[WiFiLink] ❌ TCP error: {e}")
                logger.error(f"TCP send/recv error: {e}")
                return False

            finally:
                if sock:
                    try:
                        sock.close()
                    except:
                        pass

    def send_command(self, command: str, **kwargs) -> bool:
        """
        Send a command to ESP8266 via binary TCP frame.

        Translates REST command names to binary command IDs:
        - START → 0x01
        - STOP → 0x02
        - HARD_RESET → 0x03
        - FREQUENCY + frequency=Hz → 0x10 + u8(Hz)
        - FORCE + sensor=N, force=mV → 0x11 + [u8(N), u16_LE(mV)]
        - GOTO + table=N, position=mm → 0x20 + [u8(N), u16_LE(mm×10)]
        - etc.

        Args:
            command: Command name (e.g., 'START', 'FREQUENCY', 'FORCE')
            **kwargs: Command-specific parameters

        Returns:
            True if ACK received, False on timeout/error
        """
        if not self.connected:
            logger.error("Not connected to ESP8266")
            return False

        cmd_upper = command.upper()
        args = b''

        # Map REST command to binary frame
        try:
            if cmd_upper == 'START':
                frame = build_command_frame(0x01, b'')

            elif cmd_upper == 'STOP':
                frame = build_command_frame(0x02, b'')

            elif cmd_upper == 'HARD_RESET' or cmd_upper == 'HOME':
                frame = build_command_frame(0x03, b'')

            elif cmd_upper == 'FREQUENCY':
                freq_hz = int(kwargs.get('frequency', 0))
                freq_hz = max(0, min(100, freq_hz))  # Clamp 0-100 Hz
                frame = build_command_frame(0x10, bytes([freq_hz]))

            elif cmd_upper == 'FORCE':
                # FORCE can be global (all cells) or per-cell
                sensor = kwargs.get('sensor')
                force_mv = int(kwargs.get('force', 0))
                force_mv = max(0, min(50000, force_mv))  # Clamp 0-50000 mV

                if sensor is not None:
                    # Per-cell: SET_FORCE (0x11)
                    sensor = int(sensor)
                    sensor = max(1, min(4, sensor))  # Clamp 1-4
                    args = bytes([sensor]) + struct.pack('<H', force_mv)
                    frame = build_command_frame(0x11, args)
                else:
                    # Global: SET_FORCE_ALL (0x12)
                    args = struct.pack('<H', force_mv)
                    frame = build_command_frame(0x12, args)

            elif cmd_upper == 'GOTO':
                table = int(kwargs.get('table', 1))
                position_mm = float(kwargs.get('position', 0))
                table = max(1, min(4, table))  # Clamp 1-4
                # Position in mm×10
                position_units = int(position_mm * 10)
                position_units = max(0, min(10000, position_units))
                args = bytes([table]) + struct.pack('<H', position_units)
                frame = build_command_frame(0x20, args)

            elif cmd_upper == 'MOTOR_BLINK':
                motor_id = int(kwargs.get('motor_id', 0))
                duration_ms = int(kwargs.get('duration_ms', 500))
                motor_id = max(0, min(255, motor_id))
                duration_ms = max(0, min(65535, duration_ms))
                args = bytes([motor_id]) + struct.pack('<H', duration_ms)
                frame = build_command_frame(0x30, args)

            elif cmd_upper == 'SCAN_DXL':
                frame = build_command_frame(0x31, b'')

            elif cmd_upper == 'SET_RESISTANCE':
                resistance_ohm = int(kwargs.get('resistance_ohm', 30))
                resistance_ohm = max(0, min(65535, resistance_ohm))
                args = struct.pack('<H', resistance_ohm)
                frame = build_command_frame(0x40, args)

            elif cmd_upper == 'STATUS' or cmd_upper == 'GET_STATUS':
                frame = build_command_frame(0xF0, b'')

            else:
                print(f"[WiFiLink] ❌ Unknown command: {cmd_upper}")
                logger.error(f"Unknown command: {cmd_upper}")
                return False

        except Exception as e:
            print(f"[WiFiLink] ❌ Frame construction error: {e}")
            logger.error(f"Frame construction error: {e}")
            return False

        print(f"[WiFiLink] Sending command: {cmd_upper}")
        logger.debug(f"Sending command: {cmd_upper} with kwargs={kwargs}")

        # Send binary frame and return ACK result
        return self.send_frame(frame)

    def get_status(self) -> Optional[Dict[str, Any]]:
        """Get ESP8266 status."""
        if not self.connected:
            logger.error("Not connected to ESP8266")
            return None

        try:
            with self._http_lock:
                response = self._session.get(
                    f"{self.base_url}/api/status",
                    headers=self.headers,
                    timeout=self.timeout
                )

            if response.status_code == 200:
                return response.json()
            else:
                logger.error(f"Failed to get status: {response.status_code}")
                return None
        except Exception as e:
            logger.error(f"Failed to get status: {e}")
            return None

    def send_line(self, command_str: str) -> bool:
        """
        Send command line to ESP8266 (backward compatibility with SerialLink).

        Accepts three formats:
        - REST names: START, STOP, HOME, HARD_RESET, GET_STATUS
        - Colon protocol (produced by protocol.py):
            SET_FREQ:50.0  → FREQUENCY frequency=50.0
            SET_FORCE:10.5 → FORCE force=10.5 (global, all cells)
            SET_FORCE:2:10.5 → FORCE force=10.5, sensor=2 (per-cell)
            GOTO:1:50 → GOTO table=1, position=50 mm
            BLINK_MOTOR:1:500 → MOTOR_BLINK motor_id=1, duration_ms=500
            SET_RESISTANCE:30 → SET_RESISTANCE resistance_ohm=30
        - Single-letter protocol: S, H, M, R, F50 (frequency), etc.

        All formats are translated to binary frames via send_command().
        """
        command_str = command_str.strip()
        if not command_str:
            return False

        upper = command_str.upper()
        print(f"[WiFiLink] Parsing: {upper}")

        # 1) Colon-delimited protocol commands (check BEFORE single-letter fallback)
        if ":" in upper:
            key, _, value = upper.partition(":")
            key = key.strip()
            value = value.strip()

            # Per-cell force: "SET_FORCE:<sensor>:<force>"
            if key == 'SET_FORCE' and ':' in value:
                sensor_str, _, force_str = value.partition(":")
                try:
                    sensor = int(sensor_str.strip())
                    force = float(force_str.strip()) if force_str.strip() else 0.0
                except ValueError:
                    sensor, force = 1, 0.0
                return self.send_command('FORCE', force=force, sensor=sensor)

            # Goto: "GOTO:<table>:<position>"
            if key == 'GOTO' and ':' in value:
                table_str, _, pos_str = value.partition(":")
                try:
                    table = int(table_str.strip())
                    position = float(pos_str.strip()) if pos_str.strip() else 0.0
                except ValueError:
                    table, position = 1, 0.0
                return self.send_command('GOTO', position=position, table=table)

            # BLINK_MOTOR: "BLINK_MOTOR:<motor_id>:<duration_ms>"
            if key == 'BLINK_MOTOR' and ':' in value:
                motor_str, _, duration_str = value.partition(":")
                try:
                    motor_id = int(motor_str.strip())
                    duration_ms = int(duration_str.strip()) if duration_str.strip() else 500
                except ValueError:
                    motor_id, duration_ms = 0, 500
                return self.send_command('MOTOR_BLINK', motor_id=motor_id, duration_ms=duration_ms)

            # SET_RESISTANCE: "SET_RESISTANCE:<resistance_ohm>" or "<board_id>:<ohm>"
            if key == 'SET_RESISTANCE':
                parts = value.split(':')
                try:
                    if len(parts) == 1:
                        resistance_ohm = int(parts[0].strip())
                        return self.send_command('SET_RESISTANCE', resistance_ohm=resistance_ohm)
                    else:
                        # Ignore board_id for now (binary protocol doesn't have per-board granularity)
                        resistance_ohm = int(parts[-1].strip())
                        return self.send_command('SET_RESISTANCE', resistance_ohm=resistance_ohm)
                except ValueError:
                    return False

            # Generic colon map: "SET_FREQ:50" → FREQUENCY, frequency=50
            colon_map = {
                'SET_FREQ': ('FREQUENCY', 'frequency', float),
                'SET_FREQUENCY': ('FREQUENCY', 'frequency', float),
                'SET_SPEED': ('SPEED', 'speed', int),
                'SET_FORCE': ('FORCE', 'force', float),  # Global force (no sensor specified)
            }

            if key in colon_map:
                rest_cmd, field, caster = colon_map[key]
                try:
                    param = caster(value) if value else 0
                except ValueError:
                    param = 0
                return self.send_command(rest_cmd, **{field: param})

        # 2) Direct REST command names
        rest_commands = {'START', 'STOP', 'HOME', 'HARD_RESET', 'STATUS',
                         'TORQUE_ON', 'TORQUE_OFF', 'SCAN_DXL'}
        if upper in rest_commands:
            return self.send_command(upper)
        if upper == 'GET_STATUS':
            return self.send_command('STATUS')

        # 3) Single-letter protocol fallback
        cmd_char = upper[0]
        cmd_param = upper[1:]
        protocol_map = {
            'H': ('HARD_RESET', {}),
            'S': ('START', {}),
            'M': ('STOP', {}),
            'R': ('HARD_RESET', {}),
            'F': ('FREQUENCY', {'frequency': float(cmd_param) if cmd_param else 0}),
            'V': ('SPEED', {'speed': int(cmd_param) if cmd_param else 0}),
        }

        if cmd_char in protocol_map:
            rest_cmd, params = protocol_map[cmd_char]
            return self.send_command(rest_cmd, **params)

        print(f"[WiFiLink] ❌ Unknown command: {upper}")
        logger.warning(f"Unknown command: {upper}")
        return False

    def write(self, data: str) -> bool:
        """
        Send raw command string to ESP8266.
        For compatibility with SerialLink interface.

        Args:
            data: Command string
        """
        return self.send_line(data)

    def read_line(self, timeout: Optional[float] = None) -> Optional[str]:
        """
        Read one response line from ESP8266.

        Phase 3 (binary): Responses are NOT buffered (send_frame returns immediately).
        This method returns None. Status comes from get_status() (HTTP cache).

        Kept for backward compatibility with SerialLink interface.
        """
        # Binary protocol: no buffered responses
        return None

    def readline(self, timeout: Optional[float] = None) -> Optional[str]:
        """Alias for read_line (compatibility)."""
        return self.read_line(timeout)

    def is_open(self) -> bool:
        """Check if connection is open."""
        return self.connected

    def open(self) -> bool:
        """Open/connect to ESP8266 (called by MachineController)."""
        return self.connect()

    def close(self) -> bool:
        """Close connection to ESP8266."""
        return self.disconnect()
