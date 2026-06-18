"""
Calibration tension -> force pour les cellules de force (INA125).

Charge `cal_330.txt` (couples <volts> <newtons>) à la racine du backend et
interpole linéairement. Le nom encode la résistance de gain (Rg = 330 ohm) ;
on pourra avoir d'autres fichiers (cal_<R>.txt) si le gain change.
"""

from pathlib import Path
from typing import List, Tuple

CAL_FILE = Path(__file__).resolve().parent.parent / "cal_330.txt"

# Points (volts, newtons) triés par tension croissante.
_points: List[Tuple[float, float]] = []


def load_calibration() -> List[Tuple[float, float]]:
    """(Re)charge la table de calibration depuis cal_330.txt."""
    global _points
    pts: List[Tuple[float, float]] = []
    try:
        for raw in CAL_FILE.read_text(encoding="utf-8").splitlines():
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.replace(",", " ").split()
            if len(parts) >= 2:
                try:
                    pts.append((float(parts[0]), float(parts[1])))
                except ValueError:
                    continue
    except OSError:
        pts = []
    pts.sort(key=lambda p: p[0])
    _points = pts
    print(f"[force_cal] {len(_points)} point(s) de calibration chargé(s) depuis {CAL_FILE.name}")
    return _points


def has_calibration() -> bool:
    return len(_points) >= 2


def volts_to_newton(v: float) -> float:
    """Convertit une tension (V) en force (N). Sans calibration -> renvoie la tension."""
    pts = _points
    if not pts:
        return v                      # pas calibré : on renvoie les volts bruts
    if len(pts) == 1:
        return pts[0][1]
    if v <= pts[0][0]:
        return pts[0][1]              # borne basse (pas d'extrapolation sauvage)
    if v >= pts[-1][0]:
        return pts[-1][1]             # borne haute
    for i in range(1, len(pts)):
        v0, n0 = pts[i - 1]
        v1, n1 = pts[i]
        if v <= v1:
            if v1 == v0:
                return n0
            t = (v - v0) / (v1 - v0)
            return n0 + t * (n1 - n0)
    return pts[-1][1]


# Chargement au démarrage du module
load_calibration()
