"""
Détection et sélection des ports série disponibles.
"""

import sys
from serial.tools import list_ports


# --- Ports série ---------------------------------------------------------

def list_serial_ports() -> list[tuple[str, str]]:
    ports = []
    for port in list_ports.comports():
        description = port.description or "Sans description"
        ports.append((port.device, description))
    return ports


def choose_serial_port(default_port: str | None = None) -> str:
    ports = list_serial_ports()

    if not ports:
        raise RuntimeError("Aucun port série détecté.")

    try:
        import questionary
    except ImportError as exc:
        raise RuntimeError(
            "Le module 'questionary' n'est pas installé. "
            "Installe-le avec : pip install questionary"
        ) from exc

    choices = []
    default_choice = None

    for device, description in ports:
        label = f"{device}  —  {description}"
        choices.append(questionary.Choice(title=label, value=device))

        if default_port is not None and device == default_port:
            default_choice = device

    selected = questionary.select(
        "Choisis le port série :",
        choices=choices,
        default=default_choice,
        qmark=">",
        pointer="➜",
    ).ask()

    if selected is None:
        print("\nSélection annulée.")
        sys.exit(0)

    return selected