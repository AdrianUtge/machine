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
    cmd_init_start,
    cmd_init_stop,
    cmd_init_status,
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
        # Validation: machine state (READY or IDLE only)
        if self.state.machine_status not in ("READY", "IDLE"):
            print(f"[goto] ❌ Machine état={self.state.machine_status}, rejette GOTO (nécessite READY/IDLE)")
            return

        # Validation: position limits
        table_key = f"table_{table}"
        if table_key in self.state.position_limits:
            limits = self.state.position_limits[table_key]
            if position < limits["min"] or position > limits["max"]:
                print(f"[goto] ❌ Table {table} position={position} hors limites [{limits['min']}, {limits['max']}]")
                return

        # Memorize target
        if table >= 1 and table <= 4:
            self.state.position_targets[table - 1] = position

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

    def set_resistance(self, resistance_ohm: int, board_id: int | None = None) -> None:
        """Switch INA125 gain by changing feedback resistor (30 Ω vs 90 Ω).

        board_id: 0 (D4, cells 0-1) or 1 (D5, cells 2-3), None = both
        """
        if resistance_ohm in (30, 90):
            self._send(cmd_set_resistance(resistance_ohm, board_id))

    def init_start(self, target_pos_mm: float | None = None, descent_rate: float | None = None) -> None:
        """Start motor initialization process."""
        if target_pos_mm is None:
            target_pos_mm = self.state.init_config.target_position_mm if self.state.init_config else 50.0
        if descent_rate is None:
            descent_rate = self.state.init_config.descent_rate_mm_per_min if self.state.init_config else 3.33
        self.state.init_status.running = True
        self._send(cmd_init_start(target_pos_mm, descent_rate))

    def init_stop(self) -> None:
        """Stop init immediately."""
        self.state.init_status.running = False
        self._send(cmd_init_stop())