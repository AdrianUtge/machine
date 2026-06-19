"""
init_config.py — Configuration dataclass for motor initialization process

Stores default parameters for the 3-phase init calibration:
- Phase 1: Fast descent to predetermined height
- Phase 2: Slow descent (10mm/3min) until force target reached
- Phase 3: Force hold/stability verification

Configuration is loaded from machine_config.ini [init] section at startup.
"""

from dataclasses import dataclass, asdict


@dataclass
class InitConfig:
    """Motor initialization configuration."""

    target_position_mm: float = 50.0
    """Depth for Phase 1 descent (mm). Default: 50.0"""

    descent_rate_mm_per_min: float = 3.33
    """Phase 2 descent speed (mm/min). 3.33 = 10mm/3min. Default: 3.33"""

    max_duration_s: int = 120
    """Maximum init duration before timeout (seconds). Default: 120"""

    auto_init_interval: int = 0
    """Auto-init frequency: 0 = manual only, N = every N measurement cycles. Default: 0"""

    def to_dict(self):
        """Convert to dictionary for serialization."""
        return asdict(self)

    @staticmethod
    def from_dict(d: dict) -> 'InitConfig':
        """Create from dictionary."""
        return InitConfig(**d)
