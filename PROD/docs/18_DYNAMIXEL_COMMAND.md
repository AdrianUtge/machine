# 18 — Commandes Dynamixel (4 Tables de guidage)

Spécification complète des commandes pour piloter les 4 tables Dynamixel (guidages linéaires,
1 tour de vis = 1 mm de déplacement).

## Résumé des commandes

| Commande | Format | Usage | Réponse |
|----------|--------|-------|---------|
| **GOTO** | `GOTO:table:position_mm` | Déplacer table à position (mm) | `ACK:GOTO` ou `ERR:GOTO_*` |
| **HOME** | `HOME` | Remontée + auto-calibration des limites | `ACK:HOME` puis `CALIB:1:min:max` ... |
| **TORQUE_ON** | `TORQUE_ON:table` | Verrouille table (immobilise) | `ACK:TORQUE_ON` |
| **TORQUE_OFF** | `TORQUE_OFF:table` | Déverrouille table (permet ajustement manuel) | `ACK:TORQUE_OFF` |
| **STATUS** | — | Inclus dans streaming (200 ms) | `POSITION:a,b,c,d` + `LOAD:a,b,c,d` |
| **LIMITS** | `LIMITS:table` | Lire limites min/max calibrées | `LIMITS:table:min_mm:max_mm` |
| **SET_SPEED** | `SET_SPEED:table:speed` | Vitesse Dynamixel (0–1023) | `ACK:SET_SPEED` |
| **SET_TORQUE_LIMIT** | `SET_TORQUE_LIMIT:table:limit` | Limite couple (0–1023) | `ACK:SET_TORQUE_LIMIT` |

---

## Commande détaillée : GOTO (déplacement)

### Syntaxe

```
GOTO:<table>:<position_mm>
```

### Paramètres

| Paramètre | Type | Plage | Description |
|-----------|------|-------|-------------|
| `table` | entier | 1–4 | Numéro de la table |
| `position_mm` | float | [min, max] | Position en mm (déduite du mécanisme) |

### Comportement

1. **Vérifications préalables** (firmware) :
   - État = READY (rejette sinon)
   - Position dans limites calibrées `[min_mm, max_mm]`
2. **Exécution** :
   - Appel `dxlGotoMm(table, position_mm)` → conversion mm → unités Dynamixel (1 mm = 1 DXL unit)
   - Envoi position-cible au moteur via le bus Dynamixel Serial1 (57600 baud)
3. **Feedback immédiat** : ACK envoyé (mouvement s'exécute en arrière-plan)
4. **Monitoring** : streaming toutes les 200 ms permet au frontend de voir le mouvement progresser

### Réponses

#### Succès

```
ACK:GOTO
```

Suivi (après ~200 ms) par le streaming POSITION mis à jour :
```
POSITION:a,b,c,d
```

Le frontend peut tracker la position actuelle contre la cible et afficher barre de progression.

#### Erreurs

```
ERR:GOTO_TABLE       # Table invalide (< 1 ou > 4)
ERR:GOTO_FORMAT      # Format position invalide
ERR:GOTO_LIMIT       # Position hors limites [min_mm, max_mm]
ERR:GOTO_STATE       # État ≠ READY (protège opération en cours)
ERR:SLAVE_OFFLINE    # Moteur/table déconnecté (SLAVE:OFFLINE)
```

### Exemple

```
→ GOTO:2:50.5
← ACK:GOTO
← POSITION:10.2,50.1,35.8,22.4   # ~100 ms après
← POSITION:10.2,50.3,35.8,22.4   # ~300 ms après
← POSITION:10.2,50.5,35.8,22.4   # Atteint (is_moving[2] → false)
```

### Protections

- **Rejet si RUNNING** : empêche GOTO accidentel durant oscillation stepper
- **Limites calibrées** : empêche surcharge/dépassement mécanique
- **Timeout implicite** : si mouvement bloqué > 10s, le backend détecte `is_moving ≈ 0` et force arrêt

---

## Commande détaillée : HOME (calibration)

### Syntaxe

```
HOME
```

### Comportement

Lors du HOME, **pour chaque table** :

1. **Remontée progressive** :
   - GOTO vers position max (~96 mm)
   - Polling load Dynamixel toutes les 50 ms
2. **Détection limite supérieure** :
   - Arrêt quand `load > TORQUE_THRESHOLD` (ex: 800/1023)
   - Sauvegarde position max = position courante
3. **Recul** : remontée de 2 mm → position = position_min = 0
4. **Statut** : émission CALIB pour chaque table

### Réponses

```
ACK:HOME
CALIB:1:0.0:94.2     # Table 1 : min=0, max=94.2
CALIB:2:0.0:95.8
CALIB:3:0.0:94.5
CALIB:4:0.0:96.0
```

Le backend parse ces réponses et met à jour `MachineState.position_limits`.

### Configuration

Torque threshold configurable dans `.machine_config.ini` :

```ini
[dynamixel]
torque_threshold = 800   # Seuil de détection limite (0–1023)
stroke_max_mm = 96       # Max a priori (sera écrasé par calibration auto)
```

---

## Commande : TORQUE_ON / TORQUE_OFF (verrouillage)

### Syntaxe

```
TORQUE_ON:<table>
TORQUE_OFF:<table>
```

### Comportement

- **TORQUE_ON** : Moteur verrouillé → immobilise la table (réaction à GOTO)
- **TORQUE_OFF** : Moteur déverrouillé → permet ajustement manuel (déboguer position, inspecter mécanisme)

### Réponses

```
ACK:TORQUE_ON
ACK:TORQUE_OFF
```

### Utilisation

Exemple : pour déboguer une table "collée" :
1. `TORQUE_OFF:2` → table 2 devient libre
2. Manipulation manuelle pour vérifier liberté de mouvement
3. `HOME` → recalibration
4. `TORQUE_ON:2` → table relockée

---

## Commande : LIMITS (lecture limites)

### Syntaxe

```
LIMITS:<table>
```

### Réponse

```
LIMITS:2:0.0:95.8
```

### Usage

Frontend affiche les limites dans la tooltip du slider (déduit de `position_limits` du backend).

---

## Commande : SET_SPEED (vitesse de déplacement)

### Syntaxe

```
SET_SPEED:<table>:<speed>
```

### Paramètres

| Paramètre | Type | Plage | Description |
|-----------|------|-------|-------------|
| `speed` | entier | 0–1023 | Vitesse Dynamixel MX-12W/MX-28 (RPM ∝ speed) |

### Comportement

Modifie le Moving Speed du moteur avant le prochain GOTO.

### Réponses

```
ACK:SET_SPEED
```

### Notes

- Valeur 0 = max speed (comportement Dynamixel)
- Valeur typique pour positions lentes : 100–200

---

## Commande : SET_TORQUE_LIMIT (couple max)

### Syntaxe

```
SET_TORQUE_LIMIT:<table>:<limit>
```

### Paramètres

| Paramètre | Type | Plage | Description |
|-----------|------|-------|-------------|
| `limit` | entier | 0–1023 | Couple max toléré |

### Réponses

```
ACK:SET_TORQUE_LIMIT
```

### Notes

- Limite de surcharge : si moteur bloqué et dépasse ce seuil, Dynamixel se coupe
- Défaut : 1023 (max). À régler si risque de casse mécanique

---

## Intégration Protocol (Texte + Binaire)

### Format texte (implémenté)

Les commandes ci-dessus utilisent le format **ligne texte** :
```
GOTO:2:50.5\n
```

Parsé dans `firmware/OPENRB150/src/main.cpp:dispatch()`.

### Format binaire (ÉTAPE 3)

Pour les commandes Dynamixel seulement (GOTO, HOME, TORQUE_*), support binaire via protocole compact :

```
Frame: [0xC] [CMD_ID] [args...] [CRC8]

GOTO:      [0xC] [0x20] [table:u8] [position×10:u16 LE] [CRC8]
HOME:      [0xC] [0x21] [CRC8]
TORQUE_ON: [0xC] [0x22] [table:u8] [CRC8]
...
```

À implémenter dans `handleBinaryCommand()` (ligne 757+).

---

## État & Feedback

### Streaming statut (200 ms)

```
POSITION:a,b,c,d           # (mm) Positions courantes des 4 tables
LOAD:a,b,c,d               # (0–1023) Charge/couple actuels
SLAVE:ONLINE|OFFLINE       # Détection moteurs présents
```

### Backend (state.py)

```python
@dataclass
class MachineState:
    positions: list[float]        # [pos1, pos2, pos3, pos4]
    targets: list[float]          # [target1, target2, target3, target4]
    loads: list[int]              # [load1, load2, load3, load4]
    position_limits: dict         # {"table_1": {"min": 0.0, "max": 96.0}, ...}
    is_moving: list[bool]         # [False, False, False, True]
```

### Frontend (ReactUI)

Widgets :
- **Slider** : min/max capped par table, valeur courante = position
- **Target indicator** : cercle/trait indiquant cible (en temps réel)
- **Status badge** : "READY" (vert) si `is_moving[table]=False`, "MOVING" (orange) sinon
- **Load meter** : jauge couple par table (warning si > 800)

---

## Codes erreur complets

| Code | Sens | Cause | Récupération |
|------|------|-------|--------------|
| `GOTO_TABLE` | Numéro table invalide | table < 1 ou > 4 | Vérifier numéro |
| `GOTO_FORMAT` | Position non-numérique | `GOTO:2:abc` | Vérifier format position |
| `GOTO_LIMIT` | Hors limites calibrées | position > max_mm | Utiliser slider capped |
| `GOTO_STATE` | État ≠ READY | GOTO en RUNNING | Attendre STOP |
| `SLAVE_OFFLINE` | Moteur déconnecté | dxlScan() n'a pas trouvé ID | HOME pour rescanner |
| `HOME_FAIL` | HOME avorté | Moteur bloqué durant remontée | Débrancher/vérifier mécanisme |

---

## Flux typique (cas nominal)

```
[1] Machine lancée → état IDLE
[2] Utilisateur lance HOME
    → ACK:HOME
    → CALIB:1:0:94.2 | CALIB:2:0:95.8 | ...
    → État READY
[3] Frontend affiche sliders capped [0, 94.2], [0, 95.8], ...
[4] Utilisateur règle table 2 slider → 50.5 mm
[5] POST /api/command/goto → GOTO:2:50.5
    → ACK:GOTO
    → Streaming POSITION commence de 10.2 → 50.5 progressivement
    → is_moving[2] = true → badge "MOVING"
[6] Quand position ≈ cible (< 0.5 mm)
    → is_moving[2] = false → badge "READY"
[7] Utilisateur peut relancer un GOTO ou arrêter
```

---

## Notes sur les vis de guidage

- **Standard linéaire** : 1 tour = 1 mm → DXL_PER_MM = 1.0f
- **Autres pas** : DXL_PER_MM = pitch_mm (ex: 2.0 pour pas 2 mm)
- Configurable dans `.machine_config.ini` si nécessaire

La calibration HOME est **robuste au pas** : elle détecte la vraie limite mécaniquement
(par torque), donc n'importe quel pas fonctionne sans changement firmware.
