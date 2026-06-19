"""
===============================================================================
FILE: comm/protocol.py
ROLE:
    Construction des commandes texte (descendant) et parsing des réponses
    (montant) du protocole ligne échangé avec l'OpenRB-150.

ARCHITECTURE:
    controller -> cmd_*()  -> "SET_FREQ:50.0"  -> lien -> ESP -> OpenRB
    controller <- parse_response("FREQ:50.0", state) <- lien <- ESP <- OpenRB

PROTOCOLE (résumé — voir docs/06_COMMUNICATION_PROTOCOL.md):
    Descendant : HOME | START[:epoch_ms] | STOP | HARD_RESET |
                 SET_FREQ:<hz> | SET_SPEED:<%> | SET_FORCE[:<cell>]:<n> |
                 GOTO:<table>:<mm> | TORQUE_ON | TORQUE_OFF | GET_STATUS
    Montant    : STATE:<mode> | FREQ:<hz> | POSITION:a,b,c,d | FORCE:a,b,c,d |
                 SLAVE:ONLINE|OFFLINE | ACK:<cmd> | DONE:<cmd> | ERROR:<code>

RESPONSIBILITIES:
    - cmd_*()      : sérialise une commande en ligne texte.
    - parse_response() : met à jour MachineState depuis une ligne reçue.

MAINTAINER NOTES:
    - Toute évolution du protocole DOIT rester synchrone avec l'ESP (main.cpp,
      buildOpenRbLine/parseOpenRbLine) ET l'OpenRB (main.cpp, dispatch/sendStatus).
===============================================================================
"""

from core.state import MachineState


# --- Construction des commandes -----------------------------------------

def cmd_home() -> str:
    return "HOME"


def cmd_start(start_time: int | None = None) -> str:
    # Au START on envoie l'heure de début de cycle (epoch ms) au node,
    # qui la mémorise et la renvoie ensuite: "START:<epoch_ms>".
    if start_time is None:
        return "START"
    return f"START:{start_time}"


def cmd_hard_reset() -> str:
    return "HARD_RESET"


def cmd_set_freq(freq_hz: float) -> str:
    return f"SET_FREQ:{freq_hz}"


def cmd_set_speed(speed_percent: int) -> str:
    return f"SET_SPEED:{speed_percent}"


def cmd_set_force(force_mv: float, sensor: int | None = None) -> str:
    """
    Send force setpoint to OpenRB (mV units, 0-3300).

    Phase 1 (current): Backend converts Newton targets → mV via calibration before calling this.
    OpenRB only works with mV (saves CPU power).

    Global (4 cells): "SET_FORCE:<force_mV>"
    Per-cell (1-4):   "SET_FORCE:<sensor>:<force_mV>"
    """
    if sensor is None:
        return f"SET_FORCE:{force_mv}"
    return f"SET_FORCE:{sensor}:{force_mv}"


def cmd_get_status() -> str:
    return "GET_STATUS"


def cmd_goto(table: int, position: float) -> str:
    # Déplacer une table (1-4) à une position (mm): "GOTO:<table>:<position>"
    return f"GOTO:{table}:{position}"


def cmd_torque(on: bool) -> str:
    # Verrouille (TORQUE_ON) / déverrouille (TORQUE_OFF) les Dynamixels.
    # TORQUE_OFF = moteurs libres pour positionnement manuel (unlock).
    return "TORQUE_ON" if on else "TORQUE_OFF"


def cmd_blink_motor(motor_id: int, duration_ms: int = 500) -> str:
    """
    Blink a Dynamixel motor's LED to identify it physically.
    Used for confirming motor-to-sensor mapping.

    motor_id: 0-3 (physical position)
    duration_ms: how long to blink (OpenRB will blink for ~this duration)
    """
    return f"BLINK_MOTOR:{motor_id}:{duration_ms}"


def cmd_set_resistance(resistance_ohm: int, board_id: int | None = None) -> str:
    """
    Switch INA125 gain by changing feedback resistor (30 Ω vs 90 Ω).
    Triggers relay switch on pins D4 (Board 0) or D5 (Board 1).

    resistance_ohm: 30 or 90
    board_id: 0 (D4, cells 0-1) or 1 (D5, cells 2-3), None = both
    """
    if resistance_ohm not in (30, 90):
        resistance_ohm = 30
    if board_id is None:
        return f"SET_RESISTANCE:{resistance_ohm}"
    return f"SET_RESISTANCE:{board_id}:{resistance_ohm}"


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
    elif key == "DXL_SCAN":
        # Parse: "DXL_SCAN:4,1,2,3,4" -> 4 motors with IDs [1,2,3,4]
        try:
            parts = value.split(",")
            count = int(parts[0])
            ids = [int(p.strip()) for p in parts[1:count+1]] if len(parts) > 1 else []
            print(f"[DXL_SCAN] Found {count} motor(s): IDs {ids}")
            if count < 4:
                print(f"[DXL_SCAN] ⚠️  WARNING: Expected 4 motors, found only {count}")
        except (ValueError, IndexError):
            print(f"[DXL_SCAN] ❌ Failed to parse: {value}")