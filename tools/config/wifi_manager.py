#!/usr/bin/env python3
"""
WiFi Manager for NodeMCU Connection
Reads configuration from setup.json and manages WiFi connectivity
"""

import json
import os
import platform
import subprocess
import logging
import requests
import time
from pathlib import Path
from typing import Optional, Dict, Any

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class WiFiManager:
    def __init__(self, config_path: str = None):
        """Initialize WiFi Manager with configuration file"""
        if config_path is None:
            # Try to find setup.json in the current directory or tools/config
            possible_paths = [
                Path("setup.json"),
                Path("config/setup.json"),
                Path(__file__).parent / "setup.json",
                Path(__file__).parent.parent.parent / "tools" / "config" / "setup.json",
            ]
            config_path = None
            for path in possible_paths:
                if path.exists():
                    config_path = str(path)
                    break
            if config_path is None:
                config_path = str(Path(__file__).parent / "setup.json")

        self.config_path = Path(config_path)
        self.config = self._load_config()
        self.current_ssid = None
        self.nodeMcu_connected = False

    def _load_config(self) -> Dict[str, Any]:
        """Load and parse setup.json configuration file"""
        if not self.config_path.exists():
            logger.warning(f"Config file not found: {self.config_path}, using empty config")
            return {}

        try:
            with open(self.config_path, 'r') as f:
                config = json.load(f)
            logger.info(f"Configuration loaded from {self.config_path}")
            return config
        except json.JSONDecodeError as e:
            logger.error(f"Invalid JSON in config file: {e}")
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
