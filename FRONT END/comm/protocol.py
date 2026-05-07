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
        # Parse positions: "10,20,30,40" -> [10.0, 20.0, 30.0, 40.0]
        try:
            parts = value.split(",")
            state.positions = [float(p.strip()) for p in parts[:4]]
            # Pad with zeros if less than 4 values
            while len(state.positions) < 4:
                state.positions.append(0.0)
        except ValueError:
            pass
    elif key == "CURRENT":
        state.motor_current = value
    elif key == "FORCE" or key == "SENSOR":
        # Parse sensors: "1.5,2.0,1.8,1.9" -> [1.5, 2.0, 1.8, 1.9]
        try:
            parts = value.split(",")
            state.sensors = [float(p.strip()) for p in parts[:4]]
            # Pad with zeros if less than 4 values
            while len(state.sensors) < 4:
                state.sensors.append(0.0)
        except ValueError:
            pass
    elif key == "ERROR":
        state.errors = value
    elif key == "SLAVE":
        state.slave_status = value
    elif key == "STATE":
        state.machine_status = value