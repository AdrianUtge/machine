# Dossier `cal/` — Calibration des cellules de force

## Structure

```
cal/
├── 330/              # Résistance de gain Rg = 330 Ohm (défaut)
│   ├── cell0.cal     # Calibration cellule 0
│   ├── cell1.cal     # Calibration cellule 1
│   ├── cell2.cal     # Calibration cellule 2
│   └── cell3.cal     # Calibration cellule 3
├── 470/              # Autre résistance si besoin
│   ├── cell0.cal
│   ├── cell1.cal
│   ├── cell2.cal
│   └── cell3.cal
└── README.md         # Ce fichier
```

## Format des fichiers `.cal`

Chaque fichier `.cal` est un fichier texte simple avec :
- Une ligne par point de calibration
- Format : `<tension_mV> <force_newtons>`
- Séparateur : espace ou tabulation
- Commentaires : lignes commençant par `#`
- Lignes vides ignorées

### Exemple : `330/cell0.cal`

```
# mV      newtons
61.0      0.00
92.0      3.36
115.9     5.03
...
388.4     20.20
```

## Configuration

Choisir la résistance active dans `.machine_config.ini` :

```ini
[calibration]
resistance = 330  # OHM — correspond au dossier cal/330/
```

## Conversion des données

Les données brutes de la machine de calibration sont en volts (V).  
Conversion en millivolts (mV) :

```
mV = V × 1000
```

Par exemple :
- `0.0610 V` → `61.0 mV`
- `0.3884 V` → `388.4 mV`

## Points calibrés vs Extrapolation

- **Données calibrées** : 0 → ~22 N (limite de la machine de calibration)
- **Extrapolation** : 22 → 50 N (continuation linéaire de la pente du dernier segment)
- **Clamp de sécurité** : maximum 50 N (capacité de la cellule)

### Algorithme

1. Si `mV < premier_point` → retourner `force_premier_point`
2. Si `mV > dernier_point` → **extrapoler linéairement** :
   - Pente = `(force_dernier - force_avant_dernier) / (mV_dernier - mV_avant_dernier)`
   - Force = `force_dernier + pente × (mV - mV_dernier)`
   - Force = `min(force, 50.0)` ← clamp
3. Sinon → **interpoler linéairement** entre les deux points

## Backend

- Module : `backend/comm/force_cal.py`
- Classe : `CalibrationManager`
- Méthode : `mV_to_newton(cell: int, mV: float) → float`

Chargement au démarrage du backend.

## Firmware OpenRB

Le firmware envoie les **mV bruts** au backend via Serial3 :

```
VOLT:61.0,92.0,115.9,136.2
```

Le backend applique la calibration et convertit en Newton.

## Phase 2 (Futur)

Quand le firmware implémentera la **boucle fermée de force** :
- Backend envoie la table de calibration à l'OpenRB
- OpenRB interpole mV → Newton localement
- OpenRB descend automatiquement jusqu'à la consigne
- Arrêt si force > 49 N (garde-fou)

## Troubleshooting

- **Forces nulles** : Pas de fichier `.cal` pour la résistance active
- **Valeurs aberrantes** : Données calibrées mal formatées ou résistance incorrecte
- **Ne monte pas à 50 N** : L'extrapolation est limitée à 50 N (clamp de sécurité)

Voir aussi : [`docs/15_LOAD_CELL_CALIBRATION.md`](../docs/15_LOAD_CELL_CALIBRATION.md)
