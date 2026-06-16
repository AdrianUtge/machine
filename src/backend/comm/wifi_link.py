"""
WiFi Link for communicating with ESP8266 via HTTP REST API.
Provides the same interface as SerialLink but over WiFi.
"""

import requests
import json
import logging
from typing import Optional, Dict, Any

logger = logging.getLogger(__name__)


class WiFiLink:
    """HTTP-based communication link to ESP8266 controller."""

    def __init__(self, ip: str, port: int = 8080, auth_token: str = "", timeout: float = 5.0):
        """
        Initialize WiFi link.

        Args:
            ip: ESP8266 IP address
            port: ESP8266 HTTP server port
            auth_token: Bearer token for authentication
            timeout: Request timeout in seconds
        """
        self.ip = ip
        self.port = port
        self.auth_token = auth_token
        self.timeout = timeout
        self.base_url = f"http://{ip}:{port}"
        self.connected = False

    @property
    def headers(self) -> Dict[str, str]:
        """Get headers with authentication."""
        return {
            "Content-Type": "application/json",
            "Authorization": f"Bearer {self.auth_token}"
        }

    def connect(self) -> bool:
        """Test connection to ESP8266."""
        try:
            url = f"{self.base_url}/api/status"
            print(f"[WiFiLink] Testing connection to: {url}")
            print(f"[WiFiLink] Timeout: {self.timeout}s")
            print(f"[WiFiLink] Auth Header: Bearer {'*' * 20}")

            response = requests.get(
                url,
                headers=self.headers,
                timeout=self.timeout
            )

            print(f"[WiFiLink] Response Status: {response.status_code}")
            print(f"[WiFiLink] Response Headers: {dict(response.headers)}")

            self.connected = (response.status_code == 200)

            if self.connected:
                print(f"[WiFiLink] ✅ Connected!")
                logger.info(f"Connected to ESP8266 at {self.ip}:{self.port}")
            else:
                print(f"[WiFiLink] ❌ ESP8266 returned status {response.status_code}")
                print(f"[WiFiLink] Response Body: {response.text}")
                logger.error(f"ESP8266 returned status {response.status_code}")

            return self.connected

        except requests.exceptions.ConnectionError as e:
            print(f"[WiFiLink] ❌ Connection Error: {e}")
            print(f"[WiFiLink] Could not reach {self.base_url}")
            logger.error(f"Failed to connect to ESP8266: {e}")
            self.connected = False
            return False

        except requests.exceptions.Timeout as e:
            print(f"[WiFiLink] ❌ Timeout Error: {e}")
            print(f"[WiFiLink] ESP8266 did not respond within {self.timeout}s")
            logger.error(f"Timeout connecting to ESP8266: {e}")
            self.connected = False
            return False

        except Exception as e:
            print(f"[WiFiLink] ❌ Unexpected Error: {e}")
            print(f"[WiFiLink] Error Type: {type(e).__name__}")
            logger.error(f"Unexpected error connecting to ESP8266: {e}")
            self.connected = False
            return False

    def disconnect(self) -> bool:
        """Disconnect from ESP8266."""
        self.connected = False
        logger.info(f"Disconnected from ESP8266")
        return True

    def send_command(self, command: str, **kwargs) -> bool:
        """
        Send a command to ESP8266.

        Args:
            command: Command name (e.g., 'START', 'STOP', 'HOME')
            **kwargs: Additional parameters (frequency, speed, preset, etc.)

        Returns:
            True if command was sent successfully
        """
        if not self.connected:
            logger.error("Not connected to ESP8266")
            return False

        try:
            payload = {"command": command.upper()}

            # Add parameters if provided
            if kwargs:
                for key, value in kwargs.items():
                    payload[key] = value

            print(f"[WiFiLink] Sending command: {payload}")
            logger.debug(f"Sending command: {payload}")

            response = requests.post(
                f"{self.base_url}/api/command",
                headers=self.headers,
                json=payload,
                timeout=self.timeout
            )

            print(f"[WiFiLink] Response: {response.status_code}")

            if response.status_code == 200:
                print(f"[WiFiLink] ✅ Command '{command}' sent successfully")
                logger.debug(f"Command '{command}' sent successfully")
                return True
            else:
                print(f"[WiFiLink] ❌ ESP8266 returned status {response.status_code}")
                logger.error(f"ESP8266 returned status {response.status_code}")
                return False

        except Exception as e:
            print(f"[WiFiLink] ❌ Failed to send command: {e}")
            logger.error(f"Failed to send command: {e}")
            return False

    def get_status(self) -> Optional[Dict[str, Any]]:
        """Get ESP8266 status."""
        if not self.connected:
            logger.error("Not connected to ESP8266")
            return None

        try:
            response = requests.get(
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
        Send command line to ESP8266 (compatibility with SerialLink).

        Accepts three formats:
        - REST names: START, STOP, HOME, HARD_RESET, GET_STATUS
        - Colon protocol (produced by protocol.py):
            SET_FREQ:50.0  → FREQUENCY  frequency=50.0
            SET_SPEED:75   → SPEED      speed=75
            SET_FORCE:10.5 → FORCE      force=10.5
        - Single-letter protocol: S, H, M, R, F50, V75
        """
        command_str = command_str.strip()
        if not command_str:
            return False

        upper = command_str.upper()
        print(f"[WiFiLink] Parsing: {upper}")

        # 1) Colon-delimited protocol commands (MUST be checked before the
        #    single-letter fallback: "SET_FREQ:50" starts with 'S' and would
        #    otherwise be mistaken for START).
        if ":" in upper:
            key, _, value = upper.partition(":")
            key = key.strip()
            value = value.strip()

            # Per-cell force: "SET_FORCE:<sensor>:<force>" (sensor 1-4)
            if key == 'SET_FORCE' and ':' in value:
                sensor_str, _, force_str = value.partition(":")
                try:
                    sensor = int(sensor_str.strip())
                    force = float(force_str.strip()) if force_str.strip() else 0.0
                except ValueError:
                    sensor, force = 1, 0.0
                return self.send_command('FORCE', force=force, sensor=sensor)

            # Start with cycle timestamp: "START:<epoch_ms>"
            # Sent as a string so the ESP echoes the exact digits (no float parsing).
            if key == 'START':
                return self.send_command('START', start_time=value)

            # Goto: "GOTO:<table>:<position>"
            if key == 'GOTO' and ':' in value:
                table_str, _, pos_str = value.partition(":")
                try:
                    table = int(table_str.strip())
                    position = float(pos_str.strip()) if pos_str.strip() else 0.0
                except ValueError:
                    table, position = 1, 0.0
                return self.send_command('GOTO', position=position, table=table)

            # name -> (REST command, json field, value caster)
            colon_map = {
                'SET_FREQ': ('FREQUENCY', 'frequency', float),
                'SET_FREQUENCY': ('FREQUENCY', 'frequency', float),
                'SET_SPEED': ('SPEED', 'speed', int),
                'SET_FORCE': ('FORCE', 'force', float),
            }

            if key in colon_map:
                rest_cmd, field, caster = colon_map[key]
                try:
                    param = caster(value) if value else 0
                except ValueError:
                    param = 0
                return self.send_command(rest_cmd, **{field: param})

        # 2) Direct REST command names (no parameter)
        rest_commands = {'START', 'STOP', 'HOME', 'HARD_RESET', 'STATUS'}
        if upper in rest_commands:
            return self.send_command(upper)
        if upper == 'GET_STATUS':
            return self.send_command('STATUS')

        # 3) Fall back to single-letter protocol parsing
        cmd_char = upper[0]
        cmd_param = upper[1:]
        protocol_map = {
            'H': ('HOME', {}),
            'S': ('START', {}),
            'M': ('STOP', {}),
            'R': ('HARD_RESET', {}),
            'F': ('FREQUENCY', {'frequency': float(cmd_param) if cmd_param else 0}),
            'V': ('SPEED', {'speed': int(cmd_param) if cmd_param else 0}),
        }

        if cmd_char in protocol_map:
            rest_cmd, params = protocol_map[cmd_char]
            return self.send_command(rest_cmd, **params)

        print(f"[WiFiLink] Unknown command: {upper}")
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
        Read response from ESP8266 (compatibility with SerialLink).

        For Phase 1, WiFi doesn't need to read responses line-by-line
        like serial does. Commands are sent and ESP8266 logs them.

        Returns None to indicate no data to parse.

        Args:
            timeout: Read timeout (ignored)

        Returns:
            None (no line-by-line responses for WiFi)
        """
        # Phase 1: Don't try to parse WiFi responses as serial protocol
        # The ESP8266 logs commands to its USB serial, that's enough
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
