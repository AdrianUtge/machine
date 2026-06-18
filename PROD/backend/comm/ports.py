"""
===============================================================================
FILE: comm/ports.py
ROLE:
    Détection et sélection interactive des ports série (mode USB / DEV).

RESPONSIBILITIES:
    - list_serial_ports() : (device, description) de tous les ports détectés.
    - choose_serial_port() : menu interactif (questionary) pour en choisir un.

DEPENDENCIES:
    - pyserial (list_ports), questionary (sélection interactive, optionnelle)

MAINTAINER NOTES:
    - L'UI React passe le port choisi directement à /api/connect ; ce menu sert
      surtout aux outils CLI / au mode DEV. /api/ports liste les ports côté API.
===============================================================================
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