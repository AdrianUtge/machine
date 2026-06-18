"""
===============================================================================
FILE: core/presets.py
ROLE:
    Presets de fréquence INTÉGRÉS (espèces animales). Lecture seule.

CONTENU:
    clé -> (libellé, fréquence_Hz). Ex : "1" -> ("Humain", 0.8 Hz).

MAINTAINER NOTES:
    - Presets utilisateur (couples fréquence+force, persistés) : voir preset_store.py.
      Ceux-ci sont figés dans le code (références biologiques du banc).
===============================================================================
"""

# --- Presets -------------------------------------------------------------

PRESETS = {
    "1": ("Humain", 0.8),
    "2": ("Boeuf", 0.4),
    "3": ("Souris", 3.7),
}