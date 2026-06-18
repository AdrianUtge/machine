"""
===============================================================================
FILE: comm/wifi_link.py
ROLE:
    Lien de communication backend <-> ESP8266 via HTTP REST (équivalent WiFi de
    SerialLink : même interface send_line/read_line pour MachineController).

ARCHITECTURE:
    MachineController -> WiFiLink --HTTP--> ESP8266 (192.168.4.1:8080) --série--> OpenRB-150

RESPONSIBILITIES:
    - POST /api/command : envoie une commande (avec jeton Bearer), bufferise les
      lignes de réponse renvoyées par l'OpenRB (relayées par l'ESP).
    - GET  /api/status  : lit le cache live de l'ESP (état/positions/forces).
    - Traduire le protocole "deux-points" (SET_FREQ:50) et mono-lettre (F50) vers
      les commandes REST JSON attendues par l'ESP.
    - Encaisser les pertes de paquets du lien WiFi via MAX_RETRIES.

DEPENDENCIES:
    - requests (session keep-alive), threading (sérialise les accès : l'ESP ne
      gère qu'UNE connexion HTTP à la fois).

MAINTAINER NOTES:
    - timeout COURT volontairement : un brownout ESP ne doit pas bloquer un thread
      FastAPI. Voir mémoire projet "ESP brownout / hub USB".
    - read_line() ne lit PAS le réseau : elle dépile les lignes déjà reçues par
      send_command() (FIFO). Le statut live passe par get_status(), pas par là.
===============================================================================
"""

import requests
import json
import logging
import threading
from typing import Optional, Dict, Any

logger = logging.getLogger(__name__)


class WiFiLink:
    """HTTP-based communication link to ESP8266 controller."""

    # Nombre d'essais sur perte de paquet (lien WiFi ESP intermittent).
    MAX_RETRIES = 3

    def __init__(self, ip: str, port: int = 8080, auth_token: str = "", timeout: float = 2.0):
        """
        Initialize WiFi link.

        Args:
            ip: ESP8266 IP address
            port: ESP8266 HTTP server port
            auth_token: Bearer token for authentication
            timeout: Request timeout in seconds (court : un brownout ESP ne doit
                     pas bloquer longtemps un thread du pool FastAPI)
        """
        self.ip = ip
        self.port = port
        self.auth_token = auth_token
        self.timeout = timeout
        self.base_url = f"http://{ip}:{port}"
        self.connected = False
        # L'ESP8266 ne gère qu'UNE connexion HTTP à la fois : sérialiser tous les
        # accès évite les "connection refused"/timeouts quand un poll de statut et
        # une commande tombent en même temps (endpoints désormais dans le threadpool).
        self._http_lock = threading.Lock()
        # Session réutilisée (keep-alive) : évite de rouvrir une socket TCP à chaque requête.
        self._session = requests.Session()
        # Lignes de réponse remontées par l'OpenRB (via l'ESP), FIFO.
        # Alimentée par send_command(), consommée par read_line().
        self._rx_lines: list[str] = []

    @property
    def headers(self) -> Dict[str, str]:
        """Get headers with authentication."""
        return {
            "Content-Type": "application/json",
            "Authorization": f"Bearer {self.auth_token}"
        }

    def connect(self) -> bool:
        """Test connection to ESP8266 (avec retries : le lien peut perdre des paquets)."""
        url = f"{self.base_url}/api/status"
        print(f"[WiFiLink] Testing connection to: {url} (timeout {self.timeout}s)")

        last_err = None
        for attempt in range(1, self.MAX_RETRIES + 1):
            try:
                with self._http_lock:
                    response = self._session.get(
                        url,
                        headers=self.headers,
                        timeout=self.timeout
                    )

                self.connected = (response.status_code == 200)
                if self.connected:
                    print(f"[WiFiLink] ✅ Connecté (essai {attempt})")
                    logger.info(f"Connected to ESP8266 at {self.ip}:{self.port}")
                else:
                    print(f"[WiFiLink] ❌ ESP a renvoyé {response.status_code}")
                    logger.error(f"ESP8266 returned status {response.status_code}")
                return self.connected

            except Exception as e:
                last_err = e
                print(f"[WiFiLink] ⚠️ connexion essai {attempt}/{self.MAX_RETRIES}: {e}")

        print(f"[WiFiLink] ❌ Impossible de joindre {self.base_url} après {self.MAX_RETRIES} essais: {last_err}")
        logger.error(f"Failed to connect to ESP8266 after {self.MAX_RETRIES} retries: {last_err}")
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

        payload = {"command": command.upper()}
        if kwargs:
            for key, value in kwargs.items():
                payload[key] = value

        print(f"[WiFiLink] Sending command: {payload}")
        logger.debug(f"Sending command: {payload}")

        # Le lien WiFi de l'ESP peut perdre des paquets (alim/RF) : on réessaie
        # quelques fois pour passer outre une perte ponctuelle plutôt que d'échouer.
        last_err = None
        for attempt in range(1, self.MAX_RETRIES + 1):
            try:
                with self._http_lock:
                    response = self._session.post(
                        f"{self.base_url}/api/command",
                        headers=self.headers,
                        json=payload,
                        timeout=self.timeout
                    )

                if response.status_code == 200:
                    # L'ESP renvoie les lignes de réponse remontées par l'OpenRB.
                    try:
                        data = response.json()
                        for ln in data.get("lines", []):
                            if ln:
                                self._rx_lines.append(str(ln))
                                print(f"[WiFiLink] < {ln}")
                    except Exception as e:
                        print(f"[WiFiLink] (no lines parsed: {e})")
                    print(f"[WiFiLink] ✅ Command '{command}' sent (essai {attempt})")
                    return True
                else:
                    print(f"[WiFiLink] ❌ ESP returned {response.status_code}")
                    logger.error(f"ESP8266 returned status {response.status_code}")
                    return False  # réponse HTTP reçue mais erreur -> ne pas réessayer

            except Exception as e:
                last_err = e
                print(f"[WiFiLink] ⚠️ essai {attempt}/{self.MAX_RETRIES} échoué: {e}")

        print(f"[WiFiLink] ❌ Échec commande '{command}' après {self.MAX_RETRIES} essais: {last_err}")
        logger.error(f"Failed to send command after {self.MAX_RETRIES} retries: {last_err}")
        return False

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

            # BLINK_MOTOR: "BLINK_MOTOR:<motor_id>:<duration_ms>"
            if key == 'BLINK_MOTOR' and ':' in value:
                motor_str, _, duration_str = value.partition(":")
                try:
                    motor_id = int(motor_str.strip())
                    duration_ms = int(duration_str.strip()) if duration_str.strip() else 500
                except ValueError:
                    motor_id, duration_ms = 0, 500
                return self.send_command('MOTOR_BLINK', motor_id=motor_id, duration_ms=duration_ms)

            # SET_RESISTANCE: "SET_RESISTANCE:<resistance_ohm>" or "SET_RESISTANCE:<board_id>:<resistance_ohm>"
            if key == 'SET_RESISTANCE' and ':' in value:
                parts = value.split(':')
                if len(parts) == 1:
                    # Format: <resistance_ohm> (both boards)
                    try:
                        resistance_ohm = int(parts[0].strip())
                        return self.send_command('SET_RESISTANCE', resistance_ohm=resistance_ohm, board_id=None)
                    except ValueError:
                        return False
                elif len(parts) >= 2:
                    # Format: <board_id>:<resistance_ohm>
                    try:
                        board_id = int(parts[0].strip())
                        resistance_ohm = int(parts[1].strip())
                        return self.send_command('SET_RESISTANCE', resistance_ohm=resistance_ohm, board_id=board_id)
                    except ValueError:
                        return False

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
        rest_commands = {'START', 'STOP', 'HOME', 'HARD_RESET', 'STATUS',
                         'TORQUE_ON', 'TORQUE_OFF'}
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
        Read one response line relayed from the OpenRB-150 (via the ESP8266).

        Phase 2: send_command() buffers the lines returned by the ESP
        (STATE/FREQ/POSITION/FORCE/SLAVE/ACK/...). read_line() pops them
        FIFO so MachineController.read_once() can parse_response() them.

        Returns the next buffered line, or None when the buffer is empty.
        """
        if self._rx_lines:
            return self._rx_lines.pop(0)
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
