"""
===============================================================================
FILE: debug/logger.py
ROLE:
    Buffers en mémoire pour le MONITEUR SÉRIE du frontend (≠ logs structurés).

ARCHITECTURE:
    controller._send/read_once -> DebugLogger.{log_tx,log_rx,add_command}
    api.py /api/logs/{commands,console} -> lit ces buffers -> React SerialMonitor

RESPONSIBILITIES:
    - command_history : dernières commandes envoyées (plafonné MAX_HISTORY).
    - console_lines   : lignes TX/RX préfixées (>/<), plafonné MAX_CONSOLE_LINES.

MAINTAINER NOTES:
    - À NE PAS confondre avec debug/logging_setup.py (logs process/stdout).
      Ici c'est un tampon circulaire exposé à l'UI, pas un système de log.
===============================================================================
"""

from config import MAX_CONSOLE_LINES, MAX_HISTORY


# --- Logger --------------------------------------------------------------

class DebugLogger:
    def __init__(self) -> None:
        self.command_history: list[str] = []
        self.console_lines: list[str] = []

    def log_tx(self, line: str) -> None:
        self.console_lines.append(f"> {line}")
        self.console_lines = self.console_lines[-MAX_CONSOLE_LINES:]

    def log_rx(self, line: str) -> None:
        self.console_lines.append(f"< {line}")
        self.console_lines = self.console_lines[-MAX_CONSOLE_LINES:]

    def add_command(self, command: str) -> None:
        self.command_history.append(command)
        self.command_history = self.command_history[-MAX_HISTORY:]