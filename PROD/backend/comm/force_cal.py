"""
Calibration des cellules de force (INA125) — mV → Newtons.

Charge les tables de calibration depuis PROD/cal/XXX/cellN.cal (où XXX = résistance OHM).
Interpolation linéaire entre points calibrés (0-22 N).
Extrapolation linéaire au-delà du dernier point (22-50 N).
Clamp de sécurité à 50 N.
"""

from pathlib import Path
from typing import List, Tuple, Dict


class CalibrationManager:
    """Gère la calibration pour les 4 cellules de force."""

    def __init__(self, resistance_ohm: int = 330):
        """
        Args:
            resistance_ohm : Résistance de gain (OHM) — détermine le dossier cal/ utilisé.
        """
        self.resistance_ohm = resistance_ohm
        self.cal_dir = Path(__file__).resolve().parent.parent.parent / "cal" / str(resistance_ohm)

        # Points de calibration par cellule : [(mV, newtons), ...]
        self.calibration_points: Dict[int, List[Tuple[float, float]]] = {
            0: [],
            1: [],
            2: [],
            3: [],
        }

        # Métadonnées pour debug
        self.loaded = False
        self.error_msg = ""

        self.load()

    def load(self) -> bool:
        """Charge les 4 fichiers de calibration. Retourne True si succès."""
        self.calibration_points = {0: [], 1: [], 2: [], 3: []}

        if not self.cal_dir.exists():
            self.error_msg = f"Dossier calibration absent: {self.cal_dir}"
            print(f"[force_cal] WARN: {self.error_msg}")
            return False

        all_loaded = True
        for cell_id in range(4):
            cal_file = self.cal_dir / f"cell{cell_id}.cal"
            if not cal_file.exists():
                print(f"[force_cal] WARN: {cal_file.name} absent")
                all_loaded = False
                continue

            pts = self._parse_cal_file(cal_file)
            if pts:
                self.calibration_points[cell_id] = pts
                print(f"[force_cal] cell{cell_id}: {len(pts)} point(s) chargé(s)")
            else:
                print(f"[force_cal] WARN: {cal_file.name} vide ou mal formaté")
                all_loaded = False

        self.loaded = all_loaded
        if all_loaded:
            print(f"[force_cal] Calibration {self.resistance_ohm}Ω chargée pour 4 cellules")

        return all_loaded

    @staticmethod
    def _parse_cal_file(path: Path) -> List[Tuple[float, float]]:
        """Parse un fichier .cal et retourne [(mV, newtons), ...] triés."""
        pts: List[Tuple[float, float]] = []
        try:
            for raw in path.read_text(encoding="utf-8").splitlines():
                line = raw.strip()
                if not line or line.startswith("#"):
                    continue
                # Accepter espace, tab, virgule comme séparateurs
                parts = line.replace(",", " ").split()
                if len(parts) >= 2:
                    try:
                        mv = float(parts[0])
                        newtons = float(parts[1])
                        pts.append((mv, newtons))
                    except ValueError:
                        continue
        except OSError as e:
            print(f"[force_cal] ERROR reading {path}: {e}")
            return []

        pts.sort(key=lambda p: p[0])
        return pts

    def mV_to_newton(self, cell: int, mv: float) -> float:
        """
        Convertit une tension (mV) en force (N) pour une cellule.

        - Interpolation linéaire entre points calibrés
        - Extrapolation linéaire au-delà du dernier point
        - Clamp à 50 N (sécurité, capacité cellule)
        - Sans calibration : retourne mV (passthrough)

        Args:
            cell : ID cellule (0-3)
            mv : Tension en millivolts

        Returns:
            Force en Newtons
        """
        if cell < 0 or cell > 3:
            return mv

        pts = self.calibration_points[cell]

        # Pas de calibration → passthrough
        if not pts:
            return mv

        # Un seul point → retourner sa valeur
        if len(pts) == 1:
            return pts[0][1]

        # mV en dessous du premier point → borne basse
        if mv <= pts[0][0]:
            return pts[0][1]

        # mV au-delà du dernier point → extrapoler
        if mv >= pts[-1][0]:
            v_last, n_last = pts[-1]
            v_prev, n_prev = pts[-2]

            # Pente du dernier segment
            if v_last == v_prev:
                pente = 0.0
            else:
                pente = (n_last - n_prev) / (v_last - v_prev)

            # Extrapolation linéaire
            n_extrap = n_last + pente * (mv - v_last)

            # Clamp à 50 N (sécurité)
            return min(n_extrap, 50.0)

        # Entre deux points → interpolation linéaire
        for i in range(1, len(pts)):
            v0, n0 = pts[i - 1]
            v1, n1 = pts[i]

            if mv <= v1:
                if v1 == v0:
                    return n0
                # Interpolation
                t = (mv - v0) / (v1 - v0)
                return n0 + t * (n1 - n0)

        # Fallback (ne devrait pas arriver ici)
        return pts[-1][1]

    def get_info(self) -> Dict:
        """Retourne des infos sur la calibration chargée."""
        return {
            "resistance_ohm": self.resistance_ohm,
            "loaded": self.loaded,
            "cells": {
                i: {
                    "points": len(self.calibration_points[i]),
                    "min_mv": self.calibration_points[i][0][0] if self.calibration_points[i] else None,
                    "max_mv": self.calibration_points[i][-1][0] if self.calibration_points[i] else None,
                    "min_newton": self.calibration_points[i][0][1] if self.calibration_points[i] else None,
                    "max_newton": self.calibration_points[i][-1][1] if self.calibration_points[i] else None,
                }
                for i in range(4)
            },
        }


# Instance globale (chargée au démarrage du backend)
_manager: CalibrationManager | None = None


def init_calibration(resistance_ohm: int = 330) -> CalibrationManager:
    """Initialise le gestionnaire de calibration global."""
    global _manager
    _manager = CalibrationManager(resistance_ohm)
    return _manager


def mV_to_newton(cell: int, mv: float) -> float:
    """Fonction helper pour convertir mV → N (utilise l'instance globale)."""
    if _manager is None:
        return mv
    return _manager.mV_to_newton(cell, mv)


def get_info() -> Dict:
    """Retourne les infos de calibration."""
    if _manager is None:
        return {"error": "Calibration not initialized"}
    return _manager.get_info()


def reload_calibration(resistance_ohm: int = 330) -> bool:
    """Recharge la calibration (utile pour les tests ou pour changer de résistance)."""
    global _manager
    _manager = CalibrationManager(resistance_ohm)
    return _manager.loaded
