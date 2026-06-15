#!/usr/bin/env python3
"""
Virtual Arduino Simulator - Serial Port
Simule un Arduino avec le protocole TIMC
"""

import subprocess
import re
import time
import os
import sys
import random
from datetime import datetime


class ArduinoSimulator:
    def __init__(self, baudrate=115200, log_file=None):
        self.baudrate = baudrate
        self.log_file = log_file or "serial_log.txt"
        self.socat_process = None
        self.port1 = None
        self.port2 = None
        self.rx_fd = None
        self.tx_fd = None
        self.running = False

        # État Arduino simulé
        self.state = "IDLE"
        self.frequency = 0.8
        self.speed = 100
        self.position = 0.0
        self.current = 0.0
        self.force = 0.0
        self.slave_status = "OK"
        self.is_homed = False

    def start_socat(self):
        """Lance socat pour créer deux PTYs connectés"""
        cmd = ["socat", "-d", "-d", "pty,raw,echo=0", "pty,raw,echo=0"]

        try:
            self.socat_process = subprocess.Popen(
                cmd,
                stderr=subprocess.PIPE,
                stdout=subprocess.PIPE,
                text=True
            )

            time.sleep(0.2)
            self.extract_ports()

            if not self.port1 or not self.port2:
                print("✗ Impossible de créer les ports avec socat")
                sys.exit(1)

        except FileNotFoundError:
            print("✗ socat n'est pas installé. Installation: brew install socat")
            sys.exit(1)

    def extract_ports(self):
        """Extrait les noms des ports de socat"""
        try:
            while True:
                line = self.socat_process.stderr.readline()
                if not line:
                    break
                if "PTY is" in line:
                    match = re.search(r'PTY is (/dev/[^\s]+)', line)
                    if match:
                        port = match.group(1)
                        if not self.port1:
                            self.port1 = port
                        elif not self.port2:
                            self.port2 = port
                            break
        except:
            pass

    def open_ports(self):
        """Ouvre le port de communication"""
        try:
            # Utiliser un seul port en lecture/écriture
            self.rx_fd = os.open(self.port1, os.O_RDWR | os.O_NONBLOCK)
            self.tx_fd = self.rx_fd  # Même port pour TX et RX
            print(f"✓ Arduino simulé attaché à {self.port1}")
        except Exception as e:
            print(f"✗ Erreur ouverture port: {e}")
            sys.exit(1)

    def log_message(self, direction, msg):
        """Log un message"""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        log_line = f"[{timestamp}] {direction}: {msg}"
        print(log_line)
        with open(self.log_file, 'a') as f:
            f.write(log_line + "\n")

    def send_response(self, response):
        """Envoie une réponse"""
        try:
            os.write(self.tx_fd, (response + "\n").encode())
            self.log_message("TX", response)
        except:
            pass

    def parse_and_handle_command(self, command_line):
        """Parse et traite une commande Arduino"""
        command_line = command_line.strip()
        if not command_line:
            return

        self.log_message("RX", command_line)

        # Parser le format COMMAND:argument
        if ':' in command_line:
            cmd, arg = command_line.split(':', 1)
            cmd = cmd.strip().upper()
            arg = arg.strip()
        else:
            cmd = command_line.strip().upper()
            arg = ""

        # Traiter les commandes
        if cmd == "HOME":
            self.is_homed = True
            self.state = "IDLE"
            self.send_response("ACK:HOME")
            self.send_response("DONE:HOME")

        elif cmd == "START":
            if not self.is_homed:
                self.send_response("ERROR:NOT_HOMED")
            else:
                self.state = "RUNNING"
                self.send_response("ACK:START")

        elif cmd == "STOP":
            self.state = "IDLE"
            self.send_response("ACK:STOP")

        elif cmd == "HARD_RESET":
            self.state = "IDLE"
            self.is_homed = False
            self.position = 0.0
            self.send_response("ACK:HARD_RESET")

        elif cmd == "SET_SPEED":
            try:
                speed = int(arg)
                if 0 <= speed <= 100:
                    self.speed = speed
                    self.send_response("ACK:SET_SPEED")
                else:
                    self.send_response("ERROR:SPEED_OUT_OF_RANGE")
            except:
                self.send_response("ERROR:INVALID_SPEED_FORMAT")

        elif cmd == "SET_FREQ":
            try:
                freq = float(arg)
                if 0 < freq <= 10:
                    self.frequency = freq
                    self.send_response("ACK:SET_FREQ")
                else:
                    self.send_response("ERROR:FREQ_OUT_OF_RANGE")
            except:
                self.send_response("ERROR:INVALID_FREQ_FORMAT")

        elif cmd == "GET_STATUS":
            # Simuler des valeurs qui changent légèrement
            if self.state == "RUNNING":
                self.position = (self.position + random.uniform(-0.1, 0.1)) % 360
                self.current = random.uniform(0.1, 2.5)
                self.force = random.uniform(0, 50)

            self.send_response(f"STATE:{self.state}")
            self.send_response(f"FREQ:{self.frequency:.3f}")
            self.send_response(f"POSITION:{self.position:.3f}")
            self.send_response(f"CURRENT:{self.current:.3f}")
            self.send_response(f"FORCE:{self.force:.3f}")
            self.send_response(f"SLAVE:{self.slave_status}")
            self.send_response("ACK:GET_STATUS")

        else:
            self.send_response("ERROR:UNKNOWN_COMMAND")

    def run(self):
        """Boucle principale"""
        print(f"\n╔════════════════════════════════════════════╗")
        print(f"║  Arduino Simulator (TIMC Protocol)         ║")
        print(f"╠════════════════════════════════════════════╣")
        print(f"║  RX Port: {self.port1:<29} ║")
        print(f"║  TX Port: {self.port2:<29} ║")
        print(f"║  Baudrate: {self.baudrate:<26} ║")
        print(f"║  Log file: {self.log_file:<28} ║")
        print(f"╠════════════════════════════════════════════╣")
        print(f"║  Envoyez des commandes au port {self.port2}  ║")
        print(f"║  Appuyez sur Ctrl+C pour arrêter          ║")
        print(f"╚════════════════════════════════════════════╝\n")

        self.running = True
        rx_buffer = ""

        try:
            while self.running:
                try:
                    # Lire les données
                    data = os.read(self.rx_fd, 4096)
                    if data:
                        rx_buffer += data.decode('utf-8', errors='replace')

                        # Traiter les lignes complètes
                        while '\n' in rx_buffer:
                            line, rx_buffer = rx_buffer.split('\n', 1)
                            self.parse_and_handle_command(line)

                except BlockingIOError:
                    pass
                except OSError as e:
                    if e.errno != 35:  # EAGAIN
                        raise

                # Vérifier que socat tourne toujours
                if self.socat_process.poll() is not None:
                    print("\n✗ socat s'est arrêté")
                    break

                time.sleep(0.05)

        except KeyboardInterrupt:
            print("\n\n✓ Arrêt de l'Arduino simulé...")
        finally:
            self.close()

    def close(self):
        """Ferme les connexions"""
        if self.rx_fd is not None:
            try:
                os.close(self.rx_fd)
            except:
                pass

        if self.tx_fd is not None:
            try:
                os.close(self.tx_fd)
            except:
                pass

        if self.socat_process:
            try:
                self.socat_process.terminate()
                self.socat_process.wait(timeout=2)
            except:
                try:
                    self.socat_process.kill()
                except:
                    pass

        self.running = False
        print(f"✓ Fermé. Logs: {self.log_file}")


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Arduino Simulator - TIMC Protocol")
    parser.add_argument(
        "-b", "--baudrate",
        type=int,
        default=115200,
        help="Baudrate (défaut: 115200)"
    )
    parser.add_argument(
        "-o", "--output",
        default="serial_log.txt",
        help="Fichier log (défaut: serial_log.txt)"
    )

    args = parser.parse_args()

    simulator = ArduinoSimulator(baudrate=args.baudrate, log_file=args.output)
    simulator.start_socat()
    simulator.open_ports()
    simulator.run()


if __name__ == "__main__":
    main()
