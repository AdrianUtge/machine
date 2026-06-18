# 05 — Contrôleur OpenRB-150 (SAMD21)

Dossier : `firmware/OPENRB150/` (PlatformIO, env `openrb-150`).
Fichier principal : `src/main.cpp`. MCU : **SAMD21G18A**.

> Référence matérielle approfondie (timers, ADC/DMA, dimensionnement, recalage
> par capteur de force) : [14_HARDWARE_REFERENCE.md](14_HARDWARE_REFERENCE.md).

## Rôle

Pur **exécutant temps réel** : reçoit les commandes ligne de l'ESP (Serial3),
pilote le stepper et les Dynamixel, lit les cellules de force, et **streame**
son état. Aucune décision de consigne (c'est le rôle des couches au-dessus).

## Câblage (résumé)

```
Stepper DM542T (anode commune, actif bas):
  D6 = PUL (pulses, générés par Timer TC3)   D7 = DIR   D8 = ENA
Cellules de force (4× INA125 -> filtre RC 1 Hz):  A1, A2, A3, A4
Bus Dynamixel (4 tables):  Serial1 (interne OpenRB-150), 57600 bauds
Lien vers l'ESP8266:       Serial3  (RX=D13, TX=D14), 19200 bauds
```

## Machine à états

```
IDLE ──HOME──▶ HOMING ──▶ READY ──START──▶ RUNNING
  ▲                                   │
  └──────────── STOP / HARD_RESET ────┘     (ERROR si commande invalide)
```

`modeStr()` renvoie `IDLE|HOMING|READY|RUNNING|ERROR` → ligne `STATE:`.

## Stepper (Timer TC3)

- Pulses générés en **interruption** par TC3 (priorité max → moins de jitter).
- `stepperSetFrequency(fRot)` : `f_pulse = fRot × STEPS_PER_REV` (3200 µsteps/tour) ;
  le timer toggle le pin à `2 × f_pulse`. `fRot` plafonné à `F_ROTATION_MAX = 10 Hz`.
- `STOP`/`HARD_RESET` coupe les pulses et désactive le driver (ENA haut).

## Dynamixel (4 tables)

- `dxlScan()` ping les IDs 1..20, garde les 4 premiers trouvés, mode POSITION.
- `GOTO:<table>:<mm>` → `setGoalPosition(mm × DXL_PER_MM)`.
- `TORQUE_OFF`/`TORQUE_ON` = déverrouille/verrouille (positionnement manuel).
- LED Dynamixel : **clignote en RUNNING**, fixe sinon (`updateDxlLeds()`).

## Force (INA125 → ADC)

- `readForceN(cell)` : moyenne d'un burst de 10 échantillons, puis
  `(counts - OFFSET) × GAIN`.
- ⚠️ **Calibration placeholder** : `FORCE_GAIN=1`, `FORCE_OFFSET=0`. À régler
  avec le capteur 50 N réel. Garde-fou prévu : `FORCE_MAX_N = 49 N` (ÉTAPE 2).

## Streaming statut (liaison permanente)

`loop()` appelle `sendStatus()` toutes les **100 ms** (`STREAM_PERIOD_MS`),
sans attendre de `GET_STATUS` :

```
STATE:<mode>
FREQ:<hz>
POSITION:a,b,c,d        (mm, 4 tables)
FORCE:a,b,c,d           (N, 4 cellules)
SLAVE:ONLINE|OFFLINE    (4 Dynamixel détectés ?)
```

L'ESP met ce burst en cache (voir [04_ESP8266.md](04_ESP8266.md)).

## ⚠️ État actuel : ÉTAPE 1

Le firmware **exécute** mais **ne fait PAS** encore :

- la **boucle fermée de force** (descente 1 pas/cycle jusqu'à la consigne,
  remontée rapide si dépassement, garde-fou 49 N) → voir les TODO `ÉTAPE 2`
  en fin de `loop()` et la section 13 de [14_HARDWARE_REFERENCE.md](14_HARDWARE_REFERENCE.md) ;
- la **calibration réelle** force (ADC→N) et la **conversion mm↔Dynamixel**
  (`DXL_PER_MM`) — ce sont des placeholders.

`SET_FORCE` ne fait que **mémoriser** la consigne pour l'instant.

## Build & flash

```bash
cd firmware/OPENRB150
pio run                 # build
pio run -t upload       # flash (upload_speed 1200)
pio device monitor -b 115200 --dtr 0 --rts 0   # logs (sans reset à l'ouverture)
```

Dépendance : `Dynamixel2Arduino` (ROBOTIS). Build flags :
`-DDYNAMIXEL_SERIAL=Serial1 -DARDUINO_SAMD_OPENRB`.

## Constantes à connaître

| Constante         | Valeur | Rôle                                  |
|-------------------|--------|---------------------------------------|
| `LINK_BAUD`       | 19200  | série vers l'ESP (doit matcher l'ESP) |
| `STREAM_PERIOD_MS`| 100    | période du burst de statut            |
| `STEPS_PER_REV`   | 3200   | µsteps/tour (config DIP DM542T)       |
| `F_ROTATION_MAX`  | 10 Hz  | fréquence d'oscillation max           |
| `DXL_BAUD`        | 57600  | bus Dynamixel                         |
| `FORCE_MAX_N`     | 49 N   | garde-fou capteur 50 N (ÉTAPE 2)      |
