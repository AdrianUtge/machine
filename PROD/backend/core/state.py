"""
===============================================================================
FILE: core/state.py
ROLE:
    Modèle de données central : l'état courant de la machine (dataclass).

ARCHITECTURE:
    parse_response()/_apply_esp_live_status() écrivent ICI ;
    get_state_dict() (api.py) sérialise ceci vers le frontend React.

RESPONSIBILITIES:
    - Consignes (frequency_hz, force_target/force_targets, t_speed_percent).
    - Mesures (positions[4], sensors[4], motor_current).
    - Statuts (errors, slave_status, machine_status) + cycle_start (epoch ms).
    - last_data_ts (monotonic) : fraîcheur des données -> slave ONLINE/OFFLINE.

MAINTAINER NOTES:
    - Les listes (positions/sensors/force_targets) sont initialisées à 4 zéros
      dans __post_init__ (dataclass : pas de mutable par défaut).
===============================================================================
"""

from dataclasses import dataclass, field
from core.init_config import InitConfig


# --- Init status ---------------------------------------------------------

@dataclass
class InitStatus:
    """Motor initialization process status."""
    running: bool = False
    phase: str = "IDLE"  # IDLE, PHASE1, PHASE2, PHASE3, COMPLETE, ERROR
    progress_percent: int = 0
    elapsed_ms: int = 0
    force_peaks: list[float] = field(default_factory=lambda: [0.0, 0.0, 0.0, 0.0])
    complete_motors: list[bool] = field(default_factory=lambda: [False, False, False, False])
    error_code: int = 0


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

    # Position of 4 tables (mm)
    positions: list[float] = None
    # Position targets for 4 tables (mm) — memorized after GOTO command
    position_targets: list[float] = None
    # Position limits [min, max] for each table (calibrated during HOME)
    position_limits: dict = None  # {"table_1": {"min": 0.0, "max": 96.0}, ...}
    # Is each table moving (for frontend UI feedback)
    is_moving: list[bool] = None
    # 4 force sensors
    sensors: list[float] = None
    # Motor current
    motor_current: str | None = None

    errors: str = "NONE"
    slave_status: str = "UNKNOWN"
    machine_status: str = "DISCONNECTED"
    # Horodatage (monotonic) de la dernière ligne reçue de l'OpenRB.
    # Sert à déduire l'état du slave : ONLINE = data reçue récemment, OFFLINE sinon.
    last_data_ts: float = 0.0

    # Init process state
    init_status: InitStatus = field(default_factory=InitStatus)
    init_config: 'InitConfig' = None  # Will be populated from machine_config.ini

    def __post_init__(self):
        if self.positions is None:
            self.positions = [0.0, 0.0, 0.0, 0.0]
        if self.position_targets is None:
            self.position_targets = [0.0, 0.0, 0.0, 0.0]
        if self.position_limits is None:
            self.position_limits = {
                "table_1": {"min": 0.0, "max": 96.0},
                "table_2": {"min": 0.0, "max": 96.0},
                "table_3": {"min": 0.0, "max": 96.0},
                "table_4": {"min": 0.0, "max": 96.0},
            }
        if self.is_moving is None:
            self.is_moving = [False, False, False, False]
        if self.sensors is None:
            self.sensors = [0.0, 0.0, 0.0, 0.0]
        if self.force_targets is None:
            self.force_targets = [0.0, 0.0, 0.0, 0.0]