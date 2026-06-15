#!/usr/bin/env python3
"""
Test script for ESP8266 WiFi API
Verifies that the REST API is working correctly
"""

import requests
import json
import sys
import argparse
from pathlib import Path
from typing import Optional


def test_esp8266_api(ip: str, port: int = 8080, token: str = ""):
    """Test ESP8266 API endpoints."""
    base_url = f"http://{ip}:{port}"
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {token}"
    }

    print("\n" + "=" * 60)
    print(f"Testing ESP8266 WiFi API at {ip}:{port}")
    print("=" * 60)

    # Test 1: Status endpoint
    print("\n[1/4] Testing GET /api/status...")
    try:
        response = requests.get(
            f"{base_url}/api/status",
            headers=headers,
            timeout=5
        )

        if response.status_code == 200:
            data = response.json()
            print("✓ Status retrieved:")
            print(f"  - IP: {data.get('ip')}")
            print(f"  - Pin State: {data.get('pin_state')}")
            print(f"  - OpenRB Connected: {data.get('openrb_connected')}")
            print(f"  - RSSI: {data.get('rssi')} dBm")
            print(f"  - Version: {data.get('version')}")
            print(f"  - Uptime: {data.get('uptime_ms')} ms")
        elif response.status_code == 401:
            print("✗ Unauthorized - check your Bearer token")
            return False
        else:
            print(f"✗ Error: {response.status_code}")
            print(f"  {response.text}")
            return False

    except requests.exceptions.ConnectionError:
        print(f"✗ Could not connect to {ip}:{port}")
        print("  Make sure ESP8266 is powered on and on the same network")
        return False
    except Exception as e:
        print(f"✗ Error: {e}")
        return False

    # Test 2: Send HIGH command
    print("\n[2/4] Testing POST /api/command (HIGH)...")
    try:
        response = requests.post(
            f"{base_url}/api/command",
            headers=headers,
            json={"command": "HIGH"},
            timeout=5
        )

        if response.status_code == 200:
            data = response.json()
            print("✓ Command sent:")
            print(f"  - Result: {data.get('result')}")
            print(f"  - Pin State: {data.get('pin_state')}")
            print(f"  - Timestamp: {data.get('timestamp')} ms")
        else:
            print(f"✗ Error: {response.status_code}")
            print(f"  {response.text}")
            return False

    except Exception as e:
        print(f"✗ Error: {e}")
        return False

    # Test 3: Send LOW command
    print("\n[3/4] Testing POST /api/command (LOW)...")
    try:
        response = requests.post(
            f"{base_url}/api/command",
            headers=headers,
            json={"command": "LOW"},
            timeout=5
        )

        if response.status_code == 200:
            data = response.json()
            print("✓ Command sent:")
            print(f"  - Result: {data.get('result')}")
            print(f"  - Pin State: {data.get('pin_state')}")
            print(f"  - Timestamp: {data.get('timestamp')} ms")
        else:
            print(f"✗ Error: {response.status_code}")
            print(f"  {response.text}")
            return False

    except Exception as e:
        print(f"✗ Error: {e}")
        return False

    # Test 4: Invalid command
    print("\n[4/4] Testing error handling (invalid command)...")
    try:
        response = requests.post(
            f"{base_url}/api/command",
            headers=headers,
            json={"command": "INVALID"},
            timeout=5
        )

        if response.status_code == 400:
            print("✓ Error correctly rejected:")
            print(f"  {response.json().get('error')}")
        else:
            print(f"⚠ Unexpected status: {response.status_code}")

    except Exception as e:
        print(f"✗ Error: {e}")
        return False

    print("\n" + "=" * 60)
    print("✓ All tests passed!")
    print("=" * 60 + "\n")
    return True


def load_config(config_path: Optional[str] = None) -> dict:
    """Load configuration from setup.json."""
    if config_path:
        path = Path(config_path)
    else:
        # Try to find setup.json
        paths = [
            Path("tools/config/setup.json"),
            Path("setup.json"),
            Path("config/setup.json"),
        ]
        path = None
        for p in paths:
            if p.exists():
                path = p
                break

    if not path or not path.exists():
        print("Error: setup.json not found")
        print("  Try: python tools/test_esp8266_api.py --ip 192.168.1.100 --token your_token")
        return {}

    with open(path) as f:
        return json.load(f)


def main():
    parser = argparse.ArgumentParser(
        description="Test ESP8266 WiFi REST API"
    )
    parser.add_argument("--ip", help="ESP8266 IP address")
    parser.add_argument("--port", type=int, default=8080, help="HTTP port (default: 8080)")
    parser.add_argument("--token", help="Bearer authentication token")
    parser.add_argument("--config", help="Path to setup.json")

    args = parser.parse_args()

    # Load from config if available
    if not args.ip:
        config = load_config(args.config)
        if not config:
            parser.print_help()
            sys.exit(1)

        args.ip = config.get("nodeMcu", {}).get("ip")
        args.port = config.get("nodeMcu", {}).get("port", 8080)
        args.token = config.get("nodeMcu", {}).get("key", "")

    if not args.ip:
        print("Error: ESP8266 IP address required")
        parser.print_help()
        sys.exit(1)

    success = test_esp8266_api(args.ip, args.port, args.token)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
