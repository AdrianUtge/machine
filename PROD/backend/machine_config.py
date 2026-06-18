"""
===============================================================================
FILE: machine_config.py
ROLE:
    Chargeur de la configuration centrale `.machine_config.ini` pour le backend.

ARCHITECTURE:
    .machine_config.ini  (racine PROD/, UNIQUE source de vérité, gitignoré)
        -> machine_config.py   (ce module : parse + expose)
            -> config.py        (constantes série)
            -> api.py / wifi_link.py (IP, port, jeton de l'ESP8266)

RESPONSIBILITIES:
    - Localiser `.machine_config.ini` en remontant l'arborescence depuis ce
      fichier jusqu'à la racine PROD/ (robuste au répertoire courant).
    - Parser le fichier (configparser) une seule fois (cache module).
    - Exposer des accès typés avec valeurs par défaut sûres : la machine doit
      pouvoir démarrer même si une clé manque (on retombe sur les défauts AP).

DEPENDENCIES:
    - configparser (stdlib)

MAINTAINER NOTES:
    - Tout NOUVEAU paramètre se déclare ICI (défaut) ET dans le .example.ini.
    - Ne JAMAIS logguer la valeur de [nodemcu].key en clair (secret partagé).
    - Si le .ini est absent, on logge un avertissement et on utilise les
      défauts (mode AP usine : 192.168.4.1:8080) — pratique en première install.
===============================================================================
"""

from __future__ import annotations

import configparser
from pathlib import Path
from typing import Optional

# Nom du fichier de config central, recherché en remontant l'arborescence.
_CONFIG_FILENAME = ".machine_config.ini"

# Valeurs par défaut = configuration "usine" de l'ESP8266 en mode Access Point.
# Elles permettent un premier démarrage sans .ini (on logge alors un warning).
_DEFAULTS = {
    "wifi": {"ssid": "NodeMCU-Control", "password": "", "timeout": "10"},
    "nodemcu": {
        "ip": "192.168.4.1",
        "port": "8080",
        "protocol": "http",
        "key": "1276371237612hj1h12387dsads8912",
        "timeout": "2",
    },
    "serial": {"port": "auto", "baudrate": "115200", "timeout": "1"},
    "esp8266": {"http_port": "8080"},
    "logging": {"level": "INFO"},
    "calibration": {"resistance": "330"},
}

# Cache module : on ne lit le fichier qu'une fois par process.
_parser: Optional[configparser.ConfigParser] = None
_config_path: Optional[Path] = None


def _find_config_file() -> Optional[Path]:
    """Remonte depuis ce fichier jusqu'à trouver `.machine_config.ini`.

    backend/machine_config.py -> backend/ -> PROD/ (où vit le .ini).
    Retourne None si introuvable (on utilisera les défauts).
    """
    here = Path(__file__).resolve()
    for folder in [here.parent, *here.parents]:
        candidate = folder / _CONFIG_FILENAME
        if candidate.is_file():
            return candidate
    return None


def _load() -> configparser.ConfigParser:
    """Charge (une seule fois) le .ini, en injectant les défauts manquants."""
    global _parser, _config_path
    if _parser is not None:
        return _parser

    parser = configparser.ConfigParser()
    # On amorce avec les défauts, puis on écrase avec le fichier réel s'il existe.
    parser.read_dict(_DEFAULTS)

    path = _find_config_file()
    if path is not None:
        parser.read(path, encoding="utf-8")
        _config_path = path
    else:
        # Pas d'exception : la machine doit pouvoir démarrer en mode usine.
        print(
            f"[machine_config] ⚠️  {_CONFIG_FILENAME} introuvable — "
            f"utilisation des valeurs par défaut (mode AP usine). "
            f"Copiez .machine_config.example.ini vers .machine_config.ini."
        )

    _parser = parser
    return parser


def config_path() -> Optional[Path]:
    """Chemin du .ini effectivement chargé (None si défauts)."""
    _load()
    return _config_path


# --- Accès typés (toujours une valeur, jamais d'exception) ------------------

def get(section: str, key: str, fallback: str = "") -> str:
    return _load().get(section, key, fallback=fallback)


def get_int(section: str, key: str, fallback: int = 0) -> int:
    try:
        return _load().getint(section, key, fallback=fallback)
    except ValueError:
        return fallback


def get_float(section: str, key: str, fallback: float = 0.0) -> float:
    try:
        return _load().getfloat(section, key, fallback=fallback)
    except ValueError:
        return fallback


# --- Vues structurées pratiques pour les consommateurs ----------------------

def nodemcu() -> dict:
    """Paramètres de l'ESP8266 (clés alignées sur l'ancien setup.json: 'nodeMcu')."""
    return {
        "ip": get("nodemcu", "ip", "192.168.4.1"),
        "port": get_int("nodemcu", "port", 8080),
        "protocol": get("nodemcu", "protocol", "http"),
        "key": get("nodemcu", "key", ""),
        "timeout": get_float("nodemcu", "timeout", 2.0),
    }


def wifi() -> dict:
    return {
        "ssid": get("wifi", "ssid", "NodeMCU-Control"),
        "password": get("wifi", "password", ""),
        "timeout": get_int("wifi", "timeout", 10),
    }


def serial() -> dict:
    return {
        "port": get("serial", "port", "auto"),
        "baudrate": get_int("serial", "baudrate", 115200),
        "timeout": get_float("serial", "timeout", 1.0),
    }


def log_level() -> str:
    return get("logging", "level", "INFO").upper()


def calibration() -> dict:
    return {
        "resistance": get_int("calibration", "resistance", 330),
    }


def as_nested_dict() -> dict:
    """Vue compatible avec l'ancien `setup.json` (clé 'nodeMcu' camelCase).

    Permet à WiFiManager / api.py de consommer la même forme qu'avant.
    """
    return {
        "wifi": wifi(),
        "nodeMcu": nodemcu(),
        "serial": serial(),
        "logging": {"level": log_level()},
    }
