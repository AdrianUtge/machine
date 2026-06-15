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

        Parses protocol commands and converts to WiFi API calls.
        Examples:
        - "M1000" → {"command": "START", "speed": 1000}
        - "M0" → {"command": "STOP"}
        - "H100" → {"command": "HOME"}
        - "F50" → {"command": "FREQUENCY", "frequency": 50}
        - "V75" → {"command": "SPEED", "speed": 75}
        """
        command_str = command_str.strip().upper()

        print(f"[WiFiLink] Parsing: {command_str}")

        if not command_str:
            return False

        # Simple protocol parsing for Phase 1
        cmd_char = command_str[0] if command_str else ''
        cmd_param = command_str[1:] if len(command_str) > 1 else ''

        # Map protocol commands to REST commands
        command_map = {
            'H': ('HOME', {}),
            'S': ('START', {}),
            'M': ('STOP', {}),
            'R': ('HARD_RESET', {}),
            'F': ('FREQUENCY', {'frequency': float(cmd_param) if cmd_param else 0}),
            'V': ('SPEED', {'speed': int(cmd_param) if cmd_param else 0}),
        }

        if cmd_char in command_map:
            rest_cmd, params = command_map[cmd_char]
            return self.send_command(rest_cmd, **params)

        print(f"[WiFiLink] Unknown protocol command: {command_str}")
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
        For now, returns the status as a JSON string.

        Args:
            timeout: Read timeout (ignored for now)

        Returns:
            Response string or None
        """
        status = self.get_status()
        if status:
            return json.dumps(status)
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
