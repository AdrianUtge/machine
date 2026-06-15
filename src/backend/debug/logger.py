"""
Stockage simple des logs console et de l'historique des commandes.
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