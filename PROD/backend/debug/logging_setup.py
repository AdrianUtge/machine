"""
===============================================================================
FILE: debug/logging_setup.py
ROLE:
    Système de logs structuré GLOBAL du backend (5 niveaux + mode --verbose).

ARCHITECTURE:
    api.py (main) -> setup_logging(verbose=...) -> handler console unique
    Tous les modules font: `from debug.logging_setup import get_logger`
                           `log = get_logger(__name__)`

NIVEAUX (du plus grave au plus bavard):
    ERROR   (40)  -> échec d'une opération
    WARNING (30)  -> situation anormale non bloquante
    INFO    (20)  -> évènements normaux (connexion, commande appliquée)
    DEBUG   (10)  -> détail des requêtes API / commandes / réponses
    TRACE   (5)   -> tout (octets série, payloads bruts) — ajouté ici

MODE VERBOSE:
    `--verbose` (ou `-v`) abaisse le niveau à DEBUG ; `-vv` à TRACE.
    Sans option : INFO (ou la valeur de [logging].level du .machine_config.ini).

RESPONSIBILITIES:
    - Définir le niveau TRACE (absent de la stdlib).
    - Fournir setup_logging() idempotent (un seul handler, pas de doublons).
    - Fournir get_logger() pour tout le backend.

MAINTAINER NOTES:
    - Le format inclut l'horodatage, le niveau et le module : lisible en prod
      comme en debug. Le mode TRACE ajoute le nom de fonction + la ligne.
===============================================================================
"""

from __future__ import annotations

import logging
import sys

# --- Niveau TRACE (plus bas que DEBUG) --------------------------------------
TRACE = 5
logging.addLevelName(TRACE, "TRACE")


def _trace(self: logging.Logger, message, *args, **kwargs):
    if self.isEnabledFor(TRACE):
        self._log(TRACE, message, args, **kwargs)


# Méthode .trace() sur tous les loggers (ex: log.trace("octets reçus: %r", buf)).
logging.Logger.trace = _trace  # type: ignore[attr-defined]

# Nom de la hiérarchie de loggers du backend.
_ROOT = "machine"

_LEVEL_BY_NAME = {
    "ERROR": logging.ERROR,
    "WARNING": logging.WARNING,
    "INFO": logging.INFO,
    "DEBUG": logging.DEBUG,
    "TRACE": TRACE,
}

_configured = False


def resolve_level(verbose: int = 0, level_name: str | None = None) -> int:
    """Détermine le niveau effectif.

    Priorité : -v/-vv (verbose) > level_name explicite > INFO.
      verbose >= 2 -> TRACE
      verbose == 1 -> DEBUG
    """
    if verbose >= 2:
        return TRACE
    if verbose == 1:
        return logging.DEBUG
    if level_name:
        return _LEVEL_BY_NAME.get(level_name.upper(), logging.INFO)
    return logging.INFO


def setup_logging(verbose: int = 0, level_name: str | None = None) -> int:
    """Configure le logger racine du backend. Idempotent. Retourne le niveau."""
    global _configured
    level = resolve_level(verbose, level_name)

    root = logging.getLogger(_ROOT)
    root.setLevel(level)

    if not _configured:
        handler = logging.StreamHandler(sys.stdout)
        # Format détaillé en TRACE (fonction:ligne), compact sinon.
        if level <= TRACE:
            fmt = "%(asctime)s %(levelname)-7s [%(name)s:%(funcName)s:%(lineno)d] %(message)s"
        else:
            fmt = "%(asctime)s %(levelname)-7s [%(name)s] %(message)s"
        handler.setFormatter(logging.Formatter(fmt, datefmt="%H:%M:%S"))
        root.addHandler(handler)
        root.propagate = False
        _configured = True
    else:
        # Re-réglage du format si le niveau change après coup.
        for h in root.handlers:
            if level <= TRACE:
                fmt = "%(asctime)s %(levelname)-7s [%(name)s:%(funcName)s:%(lineno)d] %(message)s"
            else:
                fmt = "%(asctime)s %(levelname)-7s [%(name)s] %(message)s"
            h.setFormatter(logging.Formatter(fmt, datefmt="%H:%M:%S"))

    # Aligne le logger 'machine_config' (warning .ini manquant) sur ce système.
    logging.getLogger("machine_config").setLevel(level)
    return level


def get_logger(name: str | None = None) -> logging.Logger:
    """Logger enfant du backend. `name` est typiquement __name__."""
    if not name or name == "__main__":
        return logging.getLogger(_ROOT)
    # Tout passe sous la hiérarchie 'machine.*' pour un contrôle de niveau unique.
    return logging.getLogger(f"{_ROOT}.{name}")
