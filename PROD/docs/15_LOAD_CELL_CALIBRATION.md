# 15 — Calibration des cellules de force (INA125)

> Vue d'ensemble du système de calibration des 4 cellules de force du banc de mesure.

## Architecture

```
OpenRB-150 (ADC)
     ↓ (counts → mV)
     → "VOLT:61.0,92.0,..." (mV bruts) via Serial3
     ↓
ESP8266 (relaie)
     → "GET /api/status"
     ↓
Backend (calibration)
     → mV → Newton (via table cal/XXX/cellN.cal)
     → "cell_volts_mv": [61.0, 92.0, ...]
     → "cell_forces_N": [0.00, 3.36, ...]
     ↓
Frontend (affichage)
     → Graphes, indicateurs en Newtons
```

**Principe clé :** Le firmware envoie des **données brutes (mV)**, pas des Newton convertis. La calibration vit au **backend uniquement**. Une seule source de vérité.

## Structure de calibration

```
PROD/cal/
├── 330/                      # Résistance Rg = 330 Ohm (défaut)
│   ├── cell0.cal
│   ├── cell1.cal
│   ├── cell2.cal
│   └── cell3.cal
├── 470/                      # Autre résistance si besoin
│   ├── cell0.cal
│   ├── cell1.cal
│   ├── cell2.cal
│   └── cell3.cal
└── README.md
```

### Format de fichier `.cal`

```
# Calibration cellule 0 — INA125 Rg = 330 Ohm
# Format : <tension_mV> <force_newtons>
# Données calibrées : 0 → 22 N (limite machine de calibration)
# Au-delà : extrapolation linéaire jusqu'à 50 N

# mV      newtons
61.0     0.00
92.0     3.36
115.9    5.03
136.2    6.12
...
388.4    20.20
```

**Remarques :**
- Les données calibrées s'arrêtent à ~22 N (limit machine)
- Au-delà de 22 N : **extrapolation linéaire** (pente du dernier segment)
- Clamp de sécurité à 50 N (capacité du capteur)

## Configuration

Dans `.machine_config.ini` :

```ini
[calibration]
resistance = 330  # OHM — détermine le dossier cal/XXX/ utilisé
```

Le backend charge automatiquement `PROD/cal/330/cell*.cal` au démarrage.

## Conversion des données

### ADC → mV (Firmware OpenRB)

Le firmware lit l'ADC 12-bit et convertit en mV :

```cpp
static const float ADC_VREF = 3300.0f;      // mV (SAMD21 @ 3.3V)
static const float ADC_RESOLUTION = 4096.0f; // 12-bit (0→4095)

float readForcemV(uint8_t cell) {
    uint32_t acc = 0;
    for (uint8_t i = 0; i < FORCE_BURST; i++) acc += analogRead(FORCE_PINS[cell]);
    float counts = (float)acc / FORCE_BURST;
    return (counts / ADC_RESOLUTION) * ADC_VREF;
}
```

Flux :
1. Lire ADC counts (0 → 4095)
2. Moyenne sur FORCE_BURST = 10 échantillons
3. Convertir en mV : `mV = (counts / 4096) × 3300`
4. Envoyer via série : `VOLT:61.0,92.0,115.9,136.2`

### mV → Newton (Backend)

Le backend interpole/extrapole la courbe de calibration :

```python
def mV_to_newton(cell: int, mv: float) -> float:
    """Convertir tension mV → force Newton"""
    pts = calibration_points[cell]  # [(mV, N), ...]
    
    # Entre deux points : interpolation linéaire
    if mv entre pts[i] et pts[i+1]:
        t = (mv - mV[i]) / (mV[i+1] - mV[i])
        return N[i] + t * (N[i+1] - N[i])
    
    # Au-delà du dernier point : extrapolation
    if mv >= pts[-1][0]:
        pente = (N[-1] - N[-2]) / (mV[-1] - mV[-2])
        force = N[-1] + pente * (mv - mV[-1])
        return min(force, 50.0)  # Clamp
```

## API Backend

### `GET /api/calibration/info`

Retourne les infos de calibration actuellement chargée :

```json
{
  "resistance_ohm": 330,
  "loaded": true,
  "cells": {
    "0": {
      "points": 11,
      "min_mv": 61.0,
      "max_mv": 388.4,
      "min_newton": 0.0,
      "max_newton": 20.2
    },
    "1": {...},
    "2": {...},
    "3": {...}
  }
}
```

### `GET /api/status`

Inclut maintenant deux nouveaux champs :

```json
{
  "...": "...",
  "cell_volts_mv": [61.0, 92.0, 115.9, 136.2],   // mV bruts
  "cell_forces_N": [0.00, 3.36, 5.03, 6.12],    // Forces converties
  "...": "..."
}
```

Le champ ancien `sensors` est maintenant égal à `cell_volts_mv` (compat).

## Phases de développement

### Phase 1 (ACTUELLE)

- ✅ OpenRB envoie mV bruts
- ✅ Backend fait l'interpolation/extrapolation
- ✅ Frontend affiche les forces en Newton
- ❌ Pas de boucle fermée (descente autonome)
- ❌ `SET_FORCE` = mémorise la consigne seulement

### Phase 2 (FUTUR)

- Backend envoie la table de calibration à l'OpenRB : `SET_CAL:cell:mV1:N1,mV2:N2,...`
- OpenRB interpole mV → Newton localement
- OpenRB implémente la boucle fermée :
  1. Lire la force mesurée
  2. Comparer à la consigne
  3. Descendre 1 pas/cycle si F < consigne
  4. Remonter rapidement si dépassement
  5. Arrêt si F > 49 N (garde-fou)

## Troubleshooting

### Forces nulles ou constantes

**Cause :** Pas de fichier `.cal` pour la résistance active.

```bash
# Vérifier la config :
cat PROD/.machine_config.ini | grep resistance

# Vérifier les fichiers :
ls -la PROD/cal/330/cell*.cal
```

**Solution :** Créer les fichiers de calibration manquants.

### Valeurs aberrantes ou décalées

**Cause :** Données mal formatées dans le fichier `.cal`.

```bash
# Vérifier la format :
head PROD/cal/330/cell0.cal
# Doit être : <mV> <newtons> (avec espace/tab entre)
```

**Solution :** Éditer le fichier pour corriger les valeurs.

### Ne monte pas à 50 N

**Cause :** L'extrapolation est correcte, mais le clamp automatique arrête à 50 N.

C'est normal et attendu : c'est une sécurité pour éviter de dépasser la capacité du capteur (50 N).

En Phase 2, le firmware respectera aussi ce limite via le garde-fou `FORCE_MAX_N = 49 N`.

### Changement de résistance (330 → 470 Ohm)

1. Modifier `.machine_config.ini` : `resistance = 470`
2. Créer les fichiers `cal/470/cell*.cal` (ou dupliquer depuis 330/)
3. Redémarrer le backend
4. Vérifier : `curl http://localhost:8000/api/calibration/info` → doit afficher 470

## Calibration en pratique

### Migrer des données existantes

Si vous avez une calibration depuis un autre outil (Excel, texte brut, etc.) :

1. Convertir les données en format mV/Newton
2. Créer les 4 fichiers `cal/330/cell{0,1,2,3}.cal`
3. Éditer `.machine_config.ini` : `resistance = 330`
4. Relancer le backend
5. Tester via `/api/calibration/info` et `/api/status`

### Extrapoler au-delà des données calibrées

La machine de calibration monte jusqu'à 22 N, mais les cellules peuvent aller jusqu'à 50 N.

L'**extrapolation linéaire** suppose que la relation mV ↔ Newton reste linéaire au-delà.

Pour les 28 N manquants (22 → 50 N) :
- Pente = (N[-1] - N[-2]) / (mV[-1] - mV[-2])
- À 50 N : mV = mV[-1] + (50 - N[-1]) / pente

**Important :** Cette extrapolation est une hypothèse. Si les données réelles divergent au-delà de 22 N, il faudra une machine de calibration plus puissante ou implémenter une autre courbe (polynôme, etc.).

## Voir aussi

- [`PROD/cal/README.md`](../cal/README.md) — Structure détaillée des dossiers
- [`backend/comm/force_cal.py`](../backend/comm/force_cal.py) — Implémentation du gestionnaire
- [`firmware/OPENRB150/src/main.cpp`](../firmware/OPENRB150/src/main.cpp) — Code firmware (ADC → mV)
- [05_OPENRB150.md](05_OPENRB150.md) — Détails matériel et câblage
