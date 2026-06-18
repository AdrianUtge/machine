"""
===============================================================================
FILE: config.py
ROLE:
    Constantes globales du backend (port série, débit, tailles de buffers logs).

ARCHITECTURE:
    .machine_config.ini -> machine_config.py -> config.py (ce fichier)

RESPONSIBILITIES:
    - Exposer DEFAULT_PORT / DEFAULT_BAUDRATE / DEFAULT_TIMEOUT pour le mode
      série USB (SerialLink), tirés de la section [serial] du .ini.
    - Exposer les plafonds de buffers de logs (DebugLogger).

MAINTAINER NOTES:
    - `port = auto` dans le .ini => DEFAULT_PORT vaut None : la sélection se fait
      alors interactivement (comm/ports.choose_serial_port) ou via l'UI, qui
      passe le port choisi explicitement à /api/connect.
    - Le mode WiFi n'utilise PAS ces constantes (voir machine_config.nodemcu()).
===============================================================================
"""

import machine_config

# --- Série (mode USB direct) --------------------------------------------------

_serial = machine_config.serial()

# `auto` => None : déclenche la détection/sélection du port au lieu d'un port figé.
DEFAULT_PORT = None if _serial["port"].lower() == "auto" else _serial["port"]
DEFAULT_BAUDRATE = _serial["baudrate"]
DEFAULT_TIMEOUT = _serial["timeout"]

# --- Logs ---------------------------------------------------------------------

MAX_HISTORY = 100
MAX_CONSOLE_LINES = 200
