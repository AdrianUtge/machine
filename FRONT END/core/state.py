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

    position: str | None = None
    motor_current: str | None = None
    force_sensor: str | None = None

    errors: str = "NONE"
    slave_status: str = "UNKNOWN"
    machine_status: str = "DISCONNECTED"