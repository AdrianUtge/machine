"""
===============================================================================
FILE: core/controller.py
ROLE:
    Orchestrateur métier : traduit les intentions (home/start/freq/force/goto…)
    en commandes protocole, les envoie via le lien, et tient l'état machine.

ARCHITECTURE:
    api.py -> MachineController -> protocol.cmd_*() -> link (WiFiLink|SerialLink)
                               <- parse_response() <- link.read_line()

RESPONSIBILITIES:
    - connect/disconnect, _send (log + envoi), read_once (lecture + parsing).
    - Commandes haut niveau : home/start/stop/hard_reset/set_freq/set_speed/
      set_force/goto/torque/apply_preset.
    - Mémoriser cycle_start (epoch ms) au START pour le calcul de runtime absolu.

DEPENDENCIES:
    - comm.protocol, comm.serial_link (type), core.state, core.presets, debug.logger

MAINTAINER NOTES:
    - Le contrôleur est agnostique du transport : `link` peut être WiFiLink ou
      SerialLink (même interface). NE PAS y mettre de logique réseau.
===============================================================================
"""

import time

from comm.protocol import (
    cmd_get_status,
    cmd_goto,
    cmd_hard_reset,
    cmd_home,
    cmd_set_force,
    cmd_set_freq,
    cmd_set_speed,
    cmd_start,
    cmd_torque,
    cmd_blink_motor,
    cmd_set_resistance,
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
            # Toute ligne reçue = l'OpenRB est vivant (sert au slave_status).
            self.state.last_data_ts = time.monotonic()
        return line

    def home(self) -> None:
        self._send(cmd_home())

    def start(self) -> None:
        # Heure de début de cycle (epoch ms) -> mémorisée localement ET envoyée
        # au node, qui la renverra pour calculer le runtime.
        self.state.cycle_start = int(time.time() * 1000)
        self._send(cmd_start(self.state.cycle_start))

    def hard_reset(self) -> None:
        self.state.cycle_start = None
        self._send(cmd_hard_reset())

    def set_speed(self, speed_percent: int) -> None:
        self.state.t_speed_percent = speed_percent
        self._send(cmd_set_speed(speed_percent))

    def set_frequency(self, freq_hz: float) -> None:
        self.state.preset_name = "MANUAL"
        self.state.frequency_hz = freq_hz
        self._send(cmd_set_freq(freq_hz))

    def set_force(self, force_n: float, sensor: int | None = None) -> None:
        if sensor is None:
            # Force globale: applique aux 4 capteurs
            self.state.force_target = force_n
            self.state.force_targets = [force_n] * 4
        elif 1 <= sensor <= 4:
            # Force par cellule (capteur 1-4)
            self.state.force_targets[sensor - 1] = force_n
        self._send(cmd_set_force(force_n, sensor))

    def set_force_mV(self, force_mv: float, sensor: int | None = None) -> None:
        """Send mV force setpoint to OpenRB (state already updated by caller)."""
        self._send(cmd_set_force(force_mv, sensor))

    def apply_preset(self, preset_key: str) -> bool:
        if preset_key not in PRESETS:
            return False

        preset_name, freq_hz = PRESETS[preset_key]
        self.state.preset_name = preset_name
        self.state.frequency_hz = freq_hz
        self._send(cmd_set_freq(freq_hz))
        return True

    def goto(self, table: int, position: float) -> None:
        self._send(cmd_goto(table, position))

    def torque(self, on: bool) -> None:
        # on=False -> déverrouille les moteurs (positionnement manuel).
        self._send(cmd_torque(on))

    def get_status(self) -> None:
        self._send(cmd_get_status())

    def blink_motor(self, motor_id: int, duration_ms: int = 500) -> None:
        """Blink a motor's LED to identify it physically (for mapping confirmation)."""
        if 0 <= motor_id <= 3:
            self._send(cmd_blink_motor(motor_id, duration_ms))

    def set_resistance(self, resistance_ohm: int) -> None:
        """Switch INA125 gain by changing feedback resistor (30 Ω vs 90 Ω)."""
        if resistance_ohm in (30, 90):
            self._send(cmd_set_resistance(resistance_ohm))