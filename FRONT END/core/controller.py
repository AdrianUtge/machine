"""
Contrôleur principal du front-end.
"""

from comm.protocol import (
    cmd_get_status,
    cmd_hard_reset,
    cmd_home,
    cmd_set_freq,
    cmd_set_speed,
    cmd_start,
    parse_response,
)
from comm.serial_link import SerialLink
from core.presets import PRESETS
from core.state import MachineState
from debug.logger import DebugLogger


# --- Contrôleur ----------------------------------------------------------

class MachineController:
    def __init__(self, link: SerialLink, state: MachineState, logger: DebugLogger) -> None:
        self.link = link
        self.state = state
        self.logger = logger

    def connect(self) -> bool:
        ok = self.link.open()
        self.state.machine_status = "CONNECTED" if ok else "DISCONNECTED"
        return ok

    def disconnect(self) -> None:
        self.link.close()
        self.state.machine_status = "DISCONNECTED"

    def _send(self, command: str) -> None:
        self.link.send_line(command)
        self.logger.add_command(command)
        self.logger.log_tx(command)

    def read_once(self) -> str | None:
        line = self.link.read_line()
        if line:
            self.logger.log_rx(line)
            parse_response(line, self.state)
        return line

    def home(self) -> None:
        self._send(cmd_home())

    def start(self) -> None:
        self._send(cmd_start())

    def hard_reset(self) -> None:
        self._send(cmd_hard_reset())

    def set_speed(self, speed_percent: int) -> None:
        self.state.t_speed_percent = speed_percent
        self._send(cmd_set_speed(speed_percent))

    def set_frequency(self, freq_hz: float) -> None:
        self.state.preset_name = "MANUAL"
        self.state.frequency_hz = freq_hz
        self._send(cmd_set_freq(freq_hz))

    def apply_preset(self, preset_key: str) -> bool:
        if preset_key not in PRESETS:
            return False

        preset_name, freq_hz = PRESETS[preset_key]
        self.state.preset_name = preset_name
        self.state.frequency_hz = freq_hz
        self._send(cmd_set_freq(freq_hz))
        return True

    def get_status(self) -> None:
        self._send(cmd_get_status())