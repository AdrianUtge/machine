"""
Menus console.
"""

from core.presets import PRESETS


# --- Menus ---------------------------------------------------------------

def main_menu() -> str:
    print("""
==================================================
                CONTROLE MACHINE
==================================================
1 - Home
2 - Start
3 - Hard Reset
4 - Choisir un preset
5 - Régler la fréquence
6 - Régler T_Speed
7 - Afficher l'état machine
8 - Demander état machine
9 - Debug
0 - Quitter
""")
    return input("Choix : ").strip()


def preset_menu() -> str:
    print("\n================ PRESETS ======================")
    for key, (name, freq) in PRESETS.items():
        print(f"{key} - {name} -> {freq} Hz")
    print("0 - Retour")
    print("==============================================")
    return input("Choix preset : ").strip()


def debug_menu() -> str:
    print("""
================ DEBUG ===========================
1 - Voir console série
2 - Voir historique des commandes
3 - Envoyer commande manuelle
4 - Lire une réponse
0 - Retour
""")
    return input("Choix : ").strip()