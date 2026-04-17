"""
Fonctions d'affichage console.
"""

from core.state import MachineState
from debug.logger import DebugLogger


# --- Affichages ----------------------------------------------------------

def show_machine_state(state: MachineState) -> None:
    print("\n================ ETAT MACHINE =================")
    print(f"Preset actif    : {state.preset_name}")
    print(f"Frequence       : {state.frequency_hz} Hz")
    print(f"T_Speed         : {state.t_speed_percent} %")
    print(f"Position        : {state.position}")
    print(f"Courant moteur  : {state.motor_current}")
    print(f"Force capteur   : {state.force_sensor}")
    print(f"Erreurs         : {state.errors}")
    print(f"Statut slave    : {state.slave_status}")
    print(f"Etat machine    : {state.machine_status}")
    print("==============================================\n")


def show_console_log(logger: DebugLogger) -> None:
    print("\n================ CONSOLE SERIE ================")
    for line in logger.console_lines[-30:]:
        print(line)
    print("==============================================\n")


def show_command_history(logger: DebugLogger) -> None:
    print("\n============= HISTORIQUE COMMANDES ============")
    for cmd in logger.command_history[-30:]:
        print(cmd)
    print("==============================================\n")