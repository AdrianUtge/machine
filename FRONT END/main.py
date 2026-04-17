"""
Point d'entrée du front-end console.
"""

from comm.serial_link import SerialLink
from config import DEFAULT_BAUDRATE, DEFAULT_PORT, DEFAULT_TIMEOUT
from core.controller import MachineController
from core.state import MachineState
from debug.logger import DebugLogger
from ui.display import show_command_history, show_console_log, show_machine_state
from ui.menus import debug_menu, main_menu, preset_menu
from comm.ports import choose_serial_port

# --- Initialisation ------------------------------------------------------

def build_controller() -> MachineController:
    port = choose_serial_port(default_port=DEFAULT_PORT)

    link = SerialLink(
        port=port,
        baudrate=DEFAULT_BAUDRATE,
        timeout=DEFAULT_TIMEOUT,
    )
    state = MachineState()
    logger = DebugLogger()

    controller = MachineController(link, state, logger)
    controller.connect()
    return controller


# --- Boucle debug --------------------------------------------------------

def run_debug_menu(controller: MachineController) -> None:
    while True:
        choice = debug_menu()

        if choice == "1":
            show_console_log(controller.logger)
        elif choice == "2":
            show_command_history(controller.logger)
        elif choice == "3":
            command = input("Commande manuelle : ").strip()
            if command:
                controller._send(command)
        elif choice == "4":
            controller.read_once()
        elif choice == "0":
            break
        else:
            print("Choix invalide.")


# --- Boucle principale ---------------------------------------------------

def main() -> None:
    controller = build_controller()

    while True:
        choice = main_menu()

        if choice == "1":
            controller.home()
            controller.read_once()

        elif choice == "2":
            controller.start()
            controller.read_once()

        elif choice == "3":
            controller.hard_reset()
            controller.read_once()

        elif choice == "4":
            preset_choice = preset_menu()
            if preset_choice != "0":
                ok = controller.apply_preset(preset_choice)
                if not ok:
                    print("Preset invalide.")
                controller.read_once()

        elif choice == "5":
            raw = input("Frequence en Hz : ").strip()
            try:
                freq = float(raw)
                controller.set_frequency(freq)
                controller.read_once()
            except ValueError:
                print("Valeur invalide.")

        elif choice == "6":
            raw = input("T_Speed (%) : ").strip()
            try:
                speed = int(raw)
                controller.set_speed(speed)
                controller.read_once()
            except ValueError:
                print("Valeur invalide.")

        elif choice == "7":
            show_machine_state(controller.state)

        elif choice == "8":
            controller.get_status()
            for _ in range(8):
                controller.read_once()

        elif choice == "9":
            run_debug_menu(controller)

        elif choice == "0":
            controller.disconnect()
            print("Fermeture.")
            break

        else:
            print("Choix invalide.")


# --- Main ----------------------------------------------------------------

if __name__ == "__main__":
    main()