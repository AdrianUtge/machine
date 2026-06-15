#!/usr/bin/env python3
"""
Test script - Write data to virtual serial port
"""

import serial
import time
import sys
import argparse


def test_serial(port, baudrate=9600, test_data=None):
    """Send test data to serial port"""
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"✓ Connecté à {port}")

        if test_data:
            # Send specific data
            messages = test_data.split(';')
            for msg in messages:
                msg = msg.strip()
                if msg:
                    ser.write(msg.encode() + b'\n')
                    print(f"→ Envoyé: {msg}")
                    time.sleep(0.5)
        else:
            # Interactive mode
            print("Mode interactif - Tapez du texte et appuyez sur Entrée")
            print("Tapez 'quit' pour quitter\n")

            while True:
                user_input = input("> ")
                if user_input.lower() == 'quit':
                    break
                ser.write(user_input.encode() + b'\n')

        ser.close()
        print("✓ Port fermé")

    except serial.SerialException as e:
        print(f"✗ Erreur: {e}")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description="Test virtual serial port by sending data"
    )
    parser.add_argument("port", help="Port série (ex: /dev/ttys000)")
    parser.add_argument(
        "-b", "--baudrate",
        type=int,
        default=9600,
        help="Baudrate (défaut: 9600)"
    )
    parser.add_argument(
        "-d", "--data",
        help="Données à envoyer (ex: 'hello;world') - sinon mode interactif"
    )

    args = parser.parse_args()

    test_serial(args.port, args.baudrate, args.data)


if __name__ == "__main__":
    main()
