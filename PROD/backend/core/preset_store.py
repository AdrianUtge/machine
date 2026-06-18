"""
Stockage persistant des presets utilisateur.

Un preset = un couple (frequency, force) enregistré sous un nom.
Les presets sont sauvegardés dans `preset.json` (à la racine du backend),
ce qui les rend permanents : ils survivent à un reboot de l'interface
(front) comme du serveur (backend).
"""

import json
from pathlib import Path
from threading import Lock
from typing import Dict

# preset.json à la racine du backend (src/backend/preset.json),
# indépendamment du répertoire courant d'exécution.
PRESET_FILE = Path(__file__).resolve().parent.parent / "preset.json"

_lock = Lock()


def load_presets() -> Dict[str, dict]:
    """Charge les presets depuis le fichier JSON. Retourne {} si absent/corrompu."""
    if not PRESET_FILE.exists():
        return {}
    try:
        with open(PRESET_FILE, "r", encoding="utf-8") as f:
            data = json.load(f)
        return data if isinstance(data, dict) else {}
    except (json.JSONDecodeError, OSError):
        return {}


def save_presets(presets: Dict[str, dict]) -> None:
    """Écrit les presets sur disque (écriture atomique via fichier temporaire)."""
    with _lock:
        tmp = PRESET_FILE.with_suffix(".json.tmp")
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(presets, f, indent=2, ensure_ascii=False)
        tmp.replace(PRESET_FILE)


def set_preset(name: str, frequency: float, force: float,
               forces: list | None = None) -> Dict[str, dict]:
    """Crée ou met à jour un preset, puis renvoie l'ensemble des presets.

    forces: liste optionnelle de 4 forces par cellule (capteur 1-4).
    """
    presets = load_presets()
    entry = {"frequency": frequency, "force": force}
    if forces is not None:
        entry["forces"] = list(forces)
    presets[name] = entry
    save_presets(presets)
    return presets


def delete_preset(name: str) -> Dict[str, dict]:
    """Supprime un preset s'il existe, puis renvoie l'ensemble des presets."""
    presets = load_presets()
    if name in presets:
        del presets[name]
        save_presets(presets)
    return presets
