#!/usr/bin/env python3
"""
===============================================================================
FILE: wifi_manager.py
ROLE:
    Détection des interfaces WiFi du PC + test de connectivité vers l'ESP8266.

ARCHITECTURE:
    .machine_config.ini  ->  WiFiManager (ce module)  ->  api.py (/api/connect,
    /api/wifi-interfaces). Importé par le backend via sys.path (api.py ajoute
    tools/config). Exécutable seul pour diagnostiquer le lien :  python3 wifi_manager.py

RESPONSIBILITIES:
    - Lister les interfaces WiFi (macOS / Linux / Windows).
    - Charger la configuration depuis `.machine_config.ini` (source unique) et
      l'exposer sous `self.config` au format historique {wifi, nodeMcu, serial}.
    - Tester/échanger avec l'ESP8266 (status/command) avec le jeton Bearer.

DEPENDENCIES:
    - requests
    - configparser (stdlib) pour lire `.machine_config.ini`

MAINTAINER NOTES:
    - Les secrets (SSID/mot de passe/jeton) viennent UNIQUEMENT du .ini gitignoré.
      Fallback legacy sur setup.json conservé pour compat, mais le .ini prime.
===============================================================================
"""

import json
import os
import platform
import subprocess
import logging
import requests
import time
import configparser
from pathlib import Path
from typing import Optional, Dict, Any

# Nom du fichier de config central (cherché en remontant l'arborescence).
_MACHINE_CONFIG = ".machine_config.ini"

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class WiFiManager:
    def __init__(self, config_path: str = None):
        """Initialise le manager.

        config_path : si fourni, force le chemin du fichier de config (utilisé
        en test). Sinon on cherche `.machine_config.ini` en remontant
        l'arborescence (source unique), avec fallback legacy sur setup.json.
        """
        self.config_path = Path(config_path) if config_path else None
        self.config = self._load_config()
        self.current_ssid = None
        self.nodeMcu_connected = False

    @staticmethod
    def _find_machine_config() -> Optional[Path]:
        """Remonte depuis ce fichier jusqu'à trouver `.machine_config.ini`."""
        here = Path(__file__).resolve()
        for folder in [here.parent, *here.parents]:
            candidate = folder / _MACHINE_CONFIG
            if candidate.is_file():
                return candidate
        return None

    @staticmethod
    def _ini_to_nested(ini: configparser.ConfigParser) -> Dict[str, Any]:
        """Convertit le .ini vers le format historique {wifi, nodeMcu, serial}.

        La clé 'nodeMcu' (camelCase) est conservée pour ne RIEN changer aux
        consommateurs existants (api.py lit config['nodeMcu']['ip'], etc.).
        """
        def g(sec, key, default=""):
            return ini.get(sec, key, fallback=default)
        return {
            "wifi": {
                "ssid": g("wifi", "ssid", "NodeMCU-Control"),
                "password": g("wifi", "password", ""),
                "timeout": int(g("wifi", "timeout", "10") or 10),
            },
            "nodeMcu": {
                "ip": g("nodemcu", "ip", "192.168.4.1"),
                "port": int(g("nodemcu", "port", "8080") or 8080),
                "key": g("nodemcu", "key", ""),
                "protocol": g("nodemcu", "protocol", "http"),
                "timeout": int(float(g("nodemcu", "timeout", "5") or 5)),
            },
            "serial": {
                "port": g("serial", "port", "auto"),
                "baudrate": int(g("serial", "baudrate", "115200") or 115200),
                "timeout": int(float(g("serial", "timeout", "1") or 1)),
            },
        }

    def _load_config(self) -> Dict[str, Any]:
        """Charge la config : `.machine_config.ini` d'abord, puis fallback JSON."""
        # 1) Source unique : .machine_config.ini
        ini_path = self.config_path if (self.config_path and self.config_path.suffix == ".ini") \
            else self._find_machine_config()
        if ini_path and ini_path.is_file():
            try:
                ini = configparser.ConfigParser()
                ini.read(ini_path, encoding="utf-8")
                self.config_path = ini_path
                logger.info(f"Configuration loaded from {ini_path}")
                return self._ini_to_nested(ini)
            except configparser.Error as e:
                logger.error(f"Invalid INI in {ini_path}: {e}")

        # 2) Fallback legacy : setup.json (compat ancien dépôt)
        json_path = self.config_path if (self.config_path and self.config_path.suffix == ".json") \
            else (Path(__file__).parent / "setup.json")
        if json_path.is_file():
            try:
                with open(json_path, "r") as f:
                    config = json.load(f)
                self.config_path = json_path
                logger.info(f"Configuration loaded from {json_path} (legacy JSON)")
                return config
            except json.JSONDecodeError as e:
                logger.error(f"Invalid JSON in config file: {e}")

        logger.warning("No config file found (.machine_config.ini / setup.json), using empty config")
        return {}

    def get_available_wifi_interfaces(self) -> list[str]:
        """Get list of available WiFi interfaces on the system"""
        system = platform.system()
        try:
            if system == "Darwin":  # macOS
                return self._get_wifi_interfaces_macos()
            elif system == "Linux":
                return self._get_wifi_interfaces_linux()
            elif system == "Windows":
                return self._get_wifi_interfaces_windows()
            else:
                logger.warning(f"Unsupported OS: {system}")
                return []
        except Exception as e:
            logger.error(f"Error getting WiFi interfaces: {e}")
            return []

    def _get_wifi_interfaces_macos(self) -> list[str]:
        """Get available WiFi interfaces on macOS"""
        try:
            result = subprocess.run(
                ["networksetup", "-listallhardwareports"],
                capture_output=True,
                text=True
            )
            interfaces = []
            for line in result.stdout.split('\n'):
                if 'Wi-Fi' in line or 'AirPort' in line:
                    # Next line should be Device: <interface>
                    continue
                if line.startswith('Device:'):
                    interface = line.split('Device:')[1].strip()
                    if interface and interface != 'N/A':
                        interfaces.append(interface)
            return interfaces
        except Exception as e:
            logger.error(f"Failed to get WiFi interfaces on macOS: {e}")
            return []

    def _get_wifi_interfaces_linux(self) -> list[str]:
        """Get available WiFi interfaces on Linux"""
        try:
            result = subprocess.run(
                ["nmcli", "dev", "status"],
                capture_output=True,
                text=True
            )
            interfaces = []
            for line in result.stdout.split('\n'):
                if 'wifi' in line.lower():
                    parts = line.split()
                    if parts:
                        interfaces.append(parts[0])
            return interfaces
        except Exception as e:
            logger.error(f"Failed to get WiFi interfaces on Linux: {e}")
            return []

    def _get_wifi_interfaces_windows(self) -> list[str]:
        """Get available WiFi interfaces on Windows"""
        try:
            result = subprocess.run(
                ["netsh", "wlan", "show", "interfaces"],
                capture_output=True,
                text=True
            )
            interfaces = []
            for line in result.stdout.split('\n'):
                if 'Name' in line and ':' in line:
                    interface = line.split(':', 1)[1].strip()
                    if interface:
                        interfaces.append(interface)
            return interfaces
        except Exception as e:
            logger.error(f"Failed to get WiFi interfaces on Windows: {e}")
            return []

    def get_connected_wifi(self) -> Optional[str]:
        """Get the SSID of the currently connected WiFi network"""
        system = platform.system()

        try:
            if system == "Darwin":  # macOS
                return self._get_wifi_macos()
            elif system == "Linux":
                return self._get_wifi_linux()
            elif system == "Windows":
                return self._get_wifi_windows()
            else:
                logger.warning(f"Unsupported OS: {system}")
                return None
        except Exception as e:
            logger.error(f"Error getting WiFi SSID: {e}")
            return None

    def _get_wifi_macos(self) -> Optional[str]:
        """Get WiFi SSID on macOS"""
        try:
            result = subprocess.run(
                ["/System/Library/PrivateFrameworks/Apple80211.framework/Versions/Current/Resources/airport", "-I"],
                capture_output=True,
                text=True
            )
            for line in result.stdout.split('\n'):
                if 'SSID:' in line:
                    ssid = line.split('SSID:')[1].strip()
                    logger.info(f"Connected WiFi (macOS): {ssid}")
                    return ssid if ssid else None
        except Exception as e:
            logger.error(f"Failed to get WiFi on macOS: {e}")
        return None

    def _get_wifi_linux(self) -> Optional[str]:
        """Get WiFi SSID on Linux"""
        try:
            result = subprocess.run(
                ["nmcli", "-t", "-f", "ACTIVE,SSID", "dev", "wifi"],
                capture_output=True,
                text=True
            )
            for line in result.stdout.split('\n'):
                if line.startswith('yes'):
                    ssid = line.split(':')[1].strip()
                    logger.info(f"Connected WiFi (Linux): {ssid}")
                    return ssid if ssid else None
        except Exception as e:
            logger.error(f"Failed to get WiFi on Linux: {e}")
        return None

    def _get_wifi_windows(self) -> Optional[str]:
        """Get WiFi SSID on Windows"""
        try:
            result = subprocess.run(
                ["netsh", "wlan", "show", "interface"],
                capture_output=True,
                text=True
            )
            for line in result.stdout.split('\n'):
                if 'SSID' in line and ':' in line:
                    ssid = line.split(':', 1)[1].strip()
                    if ssid:
                        logger.info(f"Connected WiFi (Windows): {ssid}")
                        return ssid
        except Exception as e:
            logger.error(f"Failed to get WiFi on Windows: {e}")
        return None

    def is_correct_wifi(self) -> bool:
        """Check if connected to the correct WiFi network"""
        if not self.config or 'wifi' not in self.config:
            logger.warning("No WiFi configuration found")
            return False

        self.current_ssid = self.get_connected_wifi()
        expected_ssid = self.config.get('wifi', {}).get('ssid')

        if not expected_ssid:
            logger.warning("No expected WiFi SSID in configuration")
            return False

        if self.current_ssid is None:
            logger.error("Could not determine current WiFi network")
            return False

        if self.current_ssid == expected_ssid:
            logger.info(f"✓ Connected to correct WiFi: {expected_ssid}")
            return True
        else:
            logger.warning(f"✗ WiFi mismatch. Current: {self.current_ssid}, Expected: {expected_ssid}")
            return False

    def connect_to_nodeMcu(self) -> bool:
        """Establish connection to NodeMCU using configured credentials"""
        if not self.is_correct_wifi():
            logger.error("Cannot connect to NodeMCU: Not on correct WiFi network")
            return False

        try:
            nodeMcu_config = self.config['nodeMcu']
            url = f"{nodeMcu_config['protocol']}://{nodeMcu_config['ip']}:{nodeMcu_config['port']}/api/status"
            headers = {
                'Authorization': f"Bearer {nodeMcu_config['key']}",
                'Content-Type': 'application/json'
            }

            logger.info(f"Connecting to NodeMCU at {url}...")
            response = requests.get(url, headers=headers, timeout=nodeMcu_config.get('timeout', 5))

            if response.status_code == 200:
                logger.info("✓ Successfully connected to NodeMCU")
                self.nodeMcu_connected = True
                return True
            else:
                logger.error(f"NodeMCU returned status code: {response.status_code}")
                return False

        except requests.exceptions.ConnectionError:
            logger.error(f"Could not connect to NodeMCU at {nodeMcu_config['ip']}:{nodeMcu_config['port']}")
            return False
        except Exception as e:
            logger.error(f"Error connecting to NodeMCU: {e}")
            return False

    def send_command(self, command: str) -> Optional[Dict[str, Any]]:
        """Send a command to the NodeMCU"""
        if not self.nodeMcu_connected:
            logger.error("Not connected to NodeMCU. Call connect_to_nodeMcu() first")
            return None

        try:
            nodeMcu_config = self.config['nodeMcu']
            url = f"{nodeMcu_config['protocol']}://{nodeMcu_config['ip']}:{nodeMcu_config['port']}/api/command"
            headers = {
                'Authorization': f"Bearer {nodeMcu_config['key']}",
                'Content-Type': 'application/json'
            }
            data = {'command': command}

            logger.info(f"Sending command: {command}")
            response = requests.post(url, headers=headers, json=data, timeout=5)

            if response.status_code == 200:
                logger.info(f"Command executed successfully")
                return response.json()
            else:
                logger.error(f"Command failed with status: {response.status_code}")
                return None

        except Exception as e:
            logger.error(f"Error sending command: {e}")
            return None

    def get_status(self) -> Optional[Dict[str, Any]]:
        """Get NodeMCU status"""
        if not self.nodeMcu_connected:
            logger.error("Not connected to NodeMCU")
            return None

        try:
            nodeMcu_config = self.config['nodeMcu']
            url = f"{nodeMcu_config['protocol']}://{nodeMcu_config['ip']}:{nodeMcu_config['port']}/api/status"
            headers = {'Authorization': f"Bearer {nodeMcu_config['key']}"}

            response = requests.get(url, headers=headers, timeout=5)
            if response.status_code == 200:
                return response.json()
            else:
                logger.error(f"Failed to get status: {response.status_code}")
                return None

        except Exception as e:
            logger.error(f"Error getting status: {e}")
            return None

    def print_status(self):
        """Print current connection status"""
        print("\n" + "="*60)
        print("WiFi & NodeMCU Connection Status")
        print("="*60)
        print(f"Current WiFi SSID: {self.current_ssid or 'Unknown'}")
        print(f"Expected WiFi SSID: {self.config['wifi']['ssid']}")
        print(f"WiFi Match: {'✓' if self.is_correct_wifi() else '✗'}")
        print(f"NodeMCU Connected: {'✓' if self.nodeMcu_connected else '✗'}")
        print(f"NodeMCU IP: {self.config['nodeMcu']['ip']}")
        print("="*60 + "\n")


def main():
    """Main entry point for testing"""
    try:
        manager = WiFiManager()

        # Check WiFi
        if not manager.is_correct_wifi():
            logger.error("Not connected to configured WiFi network!")
            logger.info(f"Please connect to: {manager.config['wifi']['ssid']}")
            return 1

        # Connect to NodeMCU
        if not manager.connect_to_nodeMcu():
            logger.error("Failed to connect to NodeMCU")
            return 1

        # Get status
        manager.print_status()
        status = manager.get_status()
        if status:
            logger.info(f"NodeMCU Status: {status}")

        return 0

    except KeyboardInterrupt:
        logger.info("Interrupted by user")
        return 130
    except Exception as e:
        logger.error(f"Fatal error: {e}")
        return 1


if __name__ == "__main__":
    exit(main())
