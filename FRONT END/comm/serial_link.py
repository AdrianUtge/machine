"""
Couche bas niveau de communication série.
"""

import serial


# --- Lien série ----------------------------------------------------------

class SerialLink:
    def __init__(self, port: str, baudrate: int, timeout: float) -> None:
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser: serial.Serial | None = None

    def open(self) -> bool:
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=self.timeout)
            return True
        except Exception:
            self.ser = None
            return False

    def close(self) -> None:
        if self.ser is not None:
            self.ser.close()
            self.ser = None

    def send_line(self, line: str) -> None:
        if self.ser is None:
            raise RuntimeError("Port série non ouvert.")
        self.ser.write((line.strip() + "\n").encode())

    def read_line(self) -> str | None:
        if self.ser is None:
            raise RuntimeError("Port série non ouvert.")
        raw = self.ser.readline().decode(errors="ignore").strip()
        return raw or None