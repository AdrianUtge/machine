"""
Construction des commandes et parsing des réponses Arduino.
"""

from core.state import MachineState


# --- Construction des commandes -----------------------------------------

def cmd_home() -> str:
    return "HOME"


def cmd_start() -> str:
    return "START"


def cmd_hard_reset() -> str:
    return "HARD_RESET"


def cmd_set_freq(freq_hz: float) -> str:
    return f"SET_FREQ:{freq_hz}"


def cmd_set_speed(speed_percent: int) -> str:
    return f"SET_SPEED:{speed_percent}"


def cmd_get_status() -> str:
    return "GET_STATUS"


# --- Parsing -------------------------------------------------------------

def parse_response(line: str, state: MachineState) -> None:
    if ":" not in line:
        return

    key, value = line.split(":", 1)
    key = key.strip().upper()
    value = value.strip()

    if key == "FREQ":
        try:
            state.frequency_hz = float(value)
        except ValueError:
            pass
    elif key == "POSITION":
        state.position = value
    elif key == "CURRENT":
        state.motor_current = value
    elif key == "FORCE":
        state.force_sensor = value
    elif key == "ERROR":
        state.errors = value
    elif key == "SLAVE":
        state.slave_status = value
    elif key == "STATE":
        state.machine_status = value