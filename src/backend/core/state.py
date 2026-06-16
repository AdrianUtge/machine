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
    # Force cible globale (consigne pour les 4 capteurs)
    force_target: float | None = None
    # Force cible par cellule (4 capteurs)
    force_targets: list[float] = None
    # Début de cycle (epoch ms) envoyé au node au START; None = pas démarré.
    # Sert à calculer le runtime de façon absolue (pas un compteur remis à 0).
    cycle_start: int | None = None

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
        if self.force_targets is None:
            self.force_targets = [0.0, 0.0, 0.0, 0.0]