"""
Etat courant de la machine.
"""

from dataclasses import dataclass


# --- Etat machine --------------------------------------------------------

@dataclass
class MachineState:
    preset_name: str = "MANUAL"
    frequency_hz: float | None = None
    t_speed_percent: int = 100

    # Position of 4 tables
    positions: list[float] = None
    # 4 force sensors
    sensors: list[float] = None
    # Motor current
    motor_current: str | None = None

    errors: str = "NONE"
    slave_status: str = "UNKNOWN"
    machine_status: str = "DISCONNECTED"

    def __post_init__(self):
        if self.positions is None:
            self.positions = [0.0, 0.0, 0.0, 0.0]
        if self.sensors is None:
            self.sensors = [0.0, 0.0, 0.0, 0.0]