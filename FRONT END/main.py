"""
Point d'entrée du front-end console.
"""

import time

from comm.ports import choose_serial_port
from comm.serial_link import SerialLink
from config import DEFAULT_BAUDRATE, DEFAULT_PORT, DEFAULT_TIMEOUT
from core.controller import MachineController
from core.state import MachineState
from debug.logger import DebugLogger
from ui.display import show_command_history, show_console_log, show_machine_state
from ui.menus import debug_menu, main_menu, preset_menu


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

    if controller.connect():
        print(f"Connexion série OK sur {port}")
    else:
        print(f"Erreur : impossible d'ouvrir le port {port}")

    return controller


# --- Monitor série -------------------------------------------------------

def run_command_with_monitor(
    controller: MachineController,
    command: str,
    done_token: str | None = None,
    timeout_s: float = 30.0,
) -> None:
    """
    Envoie une commande puis affiche le retour série en direct.

    La sortie automatique se fait quand l'Arduino renvoie :
    - DONE
    - DONE:<commande>
    - ou le done_token fourni
    """

    if done_token is None:
        done_token = f"DONE:{command}"

    print("\n================ MONITOR SERIE ================")
    print(f"> {command}")

    try:
        controller._send(command)
    except Exception as exc:
        print(f"Erreur envoi commande : {exc}")
        print("==============================================\n")
        return

    start_time = time.time()

    while True:
        try:
            line = controller.read_once()
        except Exception as exc:
            print(f"Erreur lecture série : {exc}")
            break

        if line:
            print(line)

            if (
                line == "DONE"
                or line == done_token
                or line.startswith("DONE:")
            ):
                print("=============== FIN COMMANDE =================\n")
                break

        if time.time() - start_time > timeout_s:
            print("Timeout : sortie automatique du monitor.")
            print("==============================================\n")
            break


# --- Lecture status ------------------------------------------------------

def request_status(controller: MachineController, lines_to_read: int = 8) -> None:
    controller.get_status()

    for _ in range(lines_to_read):
        try:
            controller.read_once()
        except Exception as exc:
            print(f"Erreur lecture série : {exc}")
            break


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
                run_command_with_monitor(
                    controller=controller,
                    command=command,
                    timeout_s=30.0,
                )

        elif choice == "4":
            try:
                line = controller.read_once()
                if line:
                    print(line)
                else:
                    print("Aucune ligne reçue.")
            except Exception as exc:
                print(f"Erreur lecture série : {exc}")

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
            run_command_with_monitor(
                controller=controller,
                command="HOME",
                timeout_s=10000.0,
            )

        elif choice == "2":
            run_command_with_monitor(
                controller=controller,
                command="START",
                timeout_s=120.0,
            )

        elif choice == "3":
            run_command_with_monitor(
                controller=controller,
                command="HARD_RESET",
                timeout_s=30.0,
            )

        elif choice == "4":
            preset_choice = preset_menu()

            if preset_choice != "0":
                ok = controller.apply_preset(preset_choice)

                if not ok:
                    print("Preset invalide.")
                else:
                    print("Preset envoyé.")
                    request_status(controller)

        elif choice == "5":
            raw = input("Frequence en Hz : ").strip()

            try:
                freq = float(raw)
                controller.set_frequency(freq)
                request_status(controller)
            except ValueError:
                print("Valeur invalide.")

        elif choice == "6":
            raw = input("T_Speed (%) : ").strip()

            try:
                speed = int(raw)
                controller.set_speed(speed)
                request_status(controller)
            except ValueError:
                print("Valeur invalide.")

        elif choice == "7":
            show_machine_state(controller.state)

        elif choice == "8":
            request_status(controller)

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