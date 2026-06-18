# 16 — Boucle Fermée de Force (ÉTAPE 2)

> Implémentation de la boucle fermée automatique pour maintenir les forces cibles sur les 4 cellules.

## Vue d'ensemble

**Problème ÉTAPE 1:** L'utilisateur devait manuellement commander `GOTO` pour ajuster les positions Dynamixel et atteindre une force désirée. Pas d'automatisation.

**Solution ÉTAPE 2:** Le firmware descend/remonte automatiquement les 4 tables pour atteindre et maintenir la force cible. Une fois `SET_FORCE:5.0` envoyé, le système s'auto-ajuste.

```
Utilisateur: "SET_FORCE:5.0 N"
      ↓
Firmware détecte force < cible
      ↓
Descend table 1 mm/cycle (descente rapide)
      ↓
À 20 mm du bas, passe en descente lente (0.1 mm/cycle)
      ↓
Force converge vers 5.0 N
      ↓
Holding + ajustements mineurs si dépassement ±5%
      ↓
Force = 5.0 N ✓
```

## Architecture mécanique

### Système oscillant

```
Mobile + 4 capteurs fixés dessus
     ↓ oscillé par stepper @ fréquence variable
     ↓
Oscille verticalement (z) à la fréquence consigne
```

**Les 4 Dynamixels NE font PAS osciller** — ils règlent la **profondeur/course** de pénétration de chaque capteur dans l'échantillon.

### Cycle d'une oscillation

```
Descente stepper (mobile baisse)
     ↓
Z = haut (tables pos initiale)
     ↓
Z = bas (capteurs touchent) ← MESURE de force
     ↓
Contact détecté (force > seuil)
     ↓
Remontée stepper (mobile remonte)
     ↓
Z > 20 mm (zone "libre") ← MOUVEMENTS Dynamixel AUTORISÉS
     ↓
Appliquer corrections si erreur de force
     ↓
Fin remontée, retour au début
```

### Fenêtre d'acquisition et mouvements

**RÈGLE CRITIQUE** : Les Dynamixels ne bougent JAMAIS pendant la mesure.

| Phase | g_stepCount | Action | Pourquoi |
|-------|-------------|--------|---------|
| Bas (mesure) | [3050, 3200] | Lire ADC, tracker peaks | Capteurs en contact, c'est LE moment |
| Remontée | < 3050 | Appliquer corrections | Mobile haut, capteurs libres, safe |

**Si on déplace les tables PENDANT la mesure** → la position change → la force change → mesure invalide → boucle fermée diverge.

## Stratégie 3-phases de contrôle

### Phase 1 : Descente rapide (approach)

**Condition:** Z > 20 mm ET force < cible - deadband

**Action:** Descendre 1 mm/cycle (STEP_DOWN_FAST)

**Objectif:** Approcher rapidement jusqu'à la zone fine

```
Z=100 mm ──(1mm/cycle)──> Z=20 mm  (80 cycles ~ 1.6 sec @ 50 Hz)
Force evolue : 0 N → ??? N (dépend pénétration initiale)
```

### Phase 2 : Descente lente (fine-tuning)

**Condition:** Z ≤ 20 mm ET force < cible - deadband

**Action:** Descendre 0.1 mm/cycle (STEP_DOWN_SLOW)

**Objectif:** Converger finement vers la cible

```
Z=20 mm ──(0.1mm/cycle)──> Z=? mm (position d'équilibre)
Force evolue : ??? N → 5.0 N ✓
```

### Phase 3 : Holding + remontée adaptatée

**Condition A - Holding:** |force - cible| ≤ 0.01 N (deadband)

**Action:** Aucune (tenir la position)

**Condition B - Remontée lente:** force > cible mais ≤ cible × 1.05

**Action:** Monter 0.2 mm/cycle (STEP_UP_SLOW)

**Condition C - Remontée rapide:** force > cible × 1.05 (>5% dépassement)

**Action:** Monter 1.0 mm/cycle (STEP_UP_FAST)

**Objectif:** Corriger les dérives légères (lent) ou les dépassements majeurs (rapide)

## Flux temporal complet

```
TEMPS:  t=0         t=20ms       t=40ms       t=60ms       t=80ms       t=100ms
        |           |            |            |            |            |
STEPPER:Start    [rampe bas]   [contact]    [rampe bas]  [remonte]    [redescend]
        |           |            |            |            |            |
g_stepCount:0→1600→2200→3050   3100→3150→3200→100→0      [repeat]

MESURE:             X            X X X X X    ✓            ✓ ✓          
                    |            ↓ ↓ ↓ ↓ ↓    |            | |
              Lire, tracker      WINDOW      Sortie      Hors WINDOW

CTRL:                            [Mesure OK]  → Calculer correction
                                              ↓
                                              Appliquer Dyn1, Dyn2, Dyn3, Dyn4
```

**Points clés:**
- Mesure se fait PENDANT la fenêtre [3050, 3200]
- Mouvements Dynamixel appliqués APRÈS la fenêtre (lors remontée)
- Correction basée sur les peaks mesurés (force max du cycle)

## Paramètres et calibration

### Fenêtre d'acquisition (paramétrabilité ÉTAPE 2+)

```cpp
static const uint16_t FORCE_WINDOW_START = 3050;  // ≈95% du cycle
static const uint16_t FORCE_WINDOW_END = 3200;    // 100% du cycle
```

**À régler empiriquement sur le matériel** :
1. Lancer oscillation 0.8 Hz
2. Logger g_stepCount toutes les 10 cycles
3. Identifier où les 4 forces deviennent > 0
4. Ajuster START/END pour englober cette zone

**Exemple :**
- Contact commence à g_stepCount ≈ 2950
- Contact se termine à g_stepCount ≈ 3200
- → SET WINDOW [2950, 3200] (légèrement avant le contact pour pas rater)

### Zone fine-tuning

```cpp
static const float FINE_TUNE_ZONE_MM = 20.0f;  // Au-dessus = descente rapide
```

**Réglage empirique** :
- Trop haut (ex: 50 mm) → approche très rapide, risque dépassement
- Trop bas (ex: 5 mm) → fine-tuning trop long, pas réactif
- **Recommandé : 15-25 mm** (ajuster selon amplitude oscillation)

### Seuil de contact

```cpp
static const float FORCE_CONTACT_THRESHOLD = 0.5f;  // 500 mN
```

**Dépend du bruit ADC** : si bruit RMS ≈ 100 mV → seuil ≈ 200-300 mV minimum.

**À régler :**
1. Lancer oscill sans contact (tables hautes)
2. Observer valeurs FORCE en bruit
3. Seuil = 2-3× bruit max observé

### Pas de descente/remontée

| Constante | Défaut | Rôle | Réglage |
|-----------|--------|------|--------|
| `STEP_DOWN_FAST_MM` | 1.0 | Descente rapide (phase 1) | ↑ = plus agressif, ↓ = plus prudent |
| `STEP_DOWN_SLOW_MM` | 0.1 | Descente lente (phase 2) | ↑ = converge vite, ↓ = très stable |
| `STEP_UP_SLOW_MM` | 0.2 | Remontée lente (±5% erreur) | ↑ = corrige vite, ↓ = très stable |
| `STEP_UP_FAST_MM` | 1.0 | Remontée rapide (>5% erreur) | ↑ = réactif, ↓ = prudent |

**Stratégie de réglage :**
```
Test 1 : Descente rapide
  SET_FORCE:5.0 → START
  Observer temps pour atteindre 5.0 N
  Si trop lent → ↑ STEP_DOWN_FAST
  Si oscille → ↓ STEP_DOWN_FAST

Test 2 : Fine-tuning
  Une fois zone atteinte, observer stabilité
  Si lent à converger → ↑ STEP_DOWN_SLOW
  Si oscille autour cible → ↓ STEP_DOWN_SLOW

Test 3 : Remontée
  SET_FORCE:3.0 après stabilisation @ 5.0
  Observer correction
  Si lente → ↑ STEP_UP_*
  Si oscille → ↓ STEP_UP_*
```

## Calibration force (ADC → Newton)

**ÉTAPE 2 utilise les calibrations existantes du backend** (voir [15_CALIBRATION_FORCE.md](15_CALIBRATION_FORCE.md)).

Le firmware envoie `VOLT:xx.x,yy.y,...` (mV bruts), le backend convertit mV → Newton via les fichiers `cal/330/cellN.cal`.

**En ÉTAPE 2, le firmware n'a besoin que de DEUX choses** :
1. **Lecture ADC** → mV (déjà fait)
2. **Streaming VOLT** → déjà implémenté

La boucle fermée compare :
- `measured_force = g_forcePeakCycle[i]` (mV, direct depuis ADC)
- `target_force = g_forceTarget[i]` (Newton, envoyé par backend via SET_FORCE)

**Conversion:** Tant que les deux sont à l'échelle mV (ou tous deux Newton), la comparaison marche. ÉTAPE 2+ (futur) enverra la table de calibration au firmware pour conversion mV → Newton côté OpenRB.

## Conversion Dynamixel (mm)

Les Dynamixels Robotis utilisent une unité interne (units), convertie en mm :

```cpp
static const float DXL_PER_MM = 1.0f;  // PLACEHOLDER à calibrer

// Utilisation :
float position_mm = dxlPositionMm(id)  // = units / DXL_PER_MM
dxlGotoMm(id, new_mm)                   // = GOTO(new_mm × DXL_PER_MM)
```

**Calibration empirique :**
1. Commande : `GOTO:1:100.0` (100 mm)
2. Mesurer déplacement réel physiquement : ex. 78 mm
3. `DXL_PER_MM = 100 / 78 ≈ 1.28`
4. Tester : `GOTO:1:100.0` → vérifie déplacement réel ≈ 100 mm

**Importance :** Une calibration incorrecte → positions aberrantes → erreurs de force imprédictibles.

## Procédure de test ÉTAPE 2

### Test 1 : Fenêtre d'acquisition

```bash
# Terminal 1 : Lancer OpenRB (USB mode)
cd PROD/firmware/OPENRB150 && pio run -t upload
pio device monitor --baud 115200

# Terminal 2 : Envoyer commandes
./run.sh

# Envoyer via curl :
curl -X POST http://localhost:8000/api/command/start
```

**Observer les logs :**
```
[Force] WINDOW entry @ stepCount=3051
[Force] Peak detected: Cell0=2.3N @ stepCount=3102
[Force] Peak detected: Cell1=2.1N @ stepCount=3105
[Force] Peak detected: Cell2=2.4N @ stepCount=3103
[Force] Peak detected: Cell3=2.2N @ stepCount=3104
[Force] WINDOW exit @ stepCount=3210
```

**Vérifications** :
- ✓ Tous les peaks dans la plage [WINDOW_START, WINDOW_END] ?
- ✓ Les 4 peaks séparés de < 10 steps (proche synchrone) ?
- ✓ Les forces varient ? (pas figées à 0 ou NaN)

### Test 2 : Stratégie 3-phases (descente rapide + fine-tuning)

```bash
# Envoyer cible de force
curl -X POST http://localhost:8000/api/command/force \
  -H "Content-Type: application/json" \
  -d '{"force": 5.0}'

# Démarrer oscillation
curl -X POST http://localhost:8000/api/command/start

# Poll statut toutes les 500 ms
while true; do
  curl http://localhost:8000/api/status | jq '.cell_forces_N, .positions'
  sleep 0.5
done
```

**Observer :**
```
Cycle 1:   Force=[0.0, 0.0, 0.0, 0.0]  Position=[96, 96, 96, 96]  Phase=DESCENT_FAST
Cycle 2:   Force=[0.3, 0.2, 0.4, 0.2]  Position=[95, 95, 95, 95]
Cycle 3:   Force=[0.8, 0.7, 0.9, 0.8]  Position=[94, 94, 94, 94]
...
Cycle 15:  Force=[4.2, 4.1, 4.3, 4.0]  Position=[20.5, 20.5, 20.5, 20.5]  Phase=DESCENT_SLOW
Cycle 16:  Force=[4.3, 4.2, 4.4, 4.1]  Position=[20.4, 20.4, 20.4, 20.4]
...
Cycle 25:  Force=[4.95, 5.05, 4.98, 5.02]  Position=[19.8, 19.8, 19.8, 19.8]  Phase=HOLDING
Cycle 26:  Force=[5.00, 5.00, 5.00, 5.00]  Position=[19.8, 19.8, 19.8, 19.8]  ✓
```

**Vérifications** :
- ✓ Positions diminuent progressivement ?
- ✓ Passage de DESCENT_FAST → DESCENT_SLOW à Z ≈ 20 mm ?
- ✓ Forces convergent vers 5.0 N (±0.05 N) ?
- ✓ Temps de convergence < 5 secondes ?

### Test 3 : Remontée (correction dépassement)

```bash
# Stabilisé à 5.0 N, puis demander 3.0 N
curl -X POST http://localhost:8000/api/command/force \
  -H "Content-Type: application/json" \
  -d '{"force": 3.0}'

# Poll statut
```

**Observer :**
```
Avant:   Force=[5.0, 5.0, 5.0, 5.0]  Position=[19.8, 19.8, 19.8, 19.8]
Après:   Force=[5.0, 5.0, 5.0, 5.0]  Position=[19.8, ...]  Phase=ASCENT_SLOW

Cycle 1: Force=[4.9, 4.8, 5.0, 4.9]  Position=[19.9, 19.9, 19.9, 19.9]  (remonte lent)
Cycle 2: Force=[3.2, 3.1, 3.3, 3.2]  Position=[21.2, 21.2, 21.2, 21.2]  ✓ ATTEINT
```

**Vérifications** :
- ✓ Remontée appliquée (Z augmente) ?
- ✓ Force diminue progressivement ?
- ✓ Converge vers 3.0 N (sans osciller excessivement) ?

## Débogage avancé

### Logs structurés (mode DEBUG)

Ajouter temporairement dans updateForceLoop() :

```cpp
if (g_mode == Mode::RUNNING && (g_stepCount % 100 == 0)) {
  Serial3.print("DEBUG_LOOP: step="); Serial3.print(g_stepCount);
  Serial3.print(" phase=[");
  for (uint8_t i = 0; i < N_TABLES; i++) {
    Serial3.print(g_forcePhase[i]); Serial3.print(",");
  }
  Serial3.print("] force=[");
  for (uint8_t i = 0; i < N_TABLES; i++) {
    Serial3.print(g_forcePeakCycle[i], 2); Serial3.print(",");
  }
  Serial3.println("]");
}
```

### Vérifie les transitions de fenêtre

```cpp
// Dans isInForceWindow() :
static uint16_t lastValidStep = 0;
if (nowInWindow && g_stepCount != lastValidStep) {
  if (g_stepCount < lastValidStep) {
    Serial3.println("[WINDOW] Cycle restart detected");
  }
  lastValidStep = g_stepCount;
}
```

### Soft-limits Dynamixel

**À implémenter dans updateForceLoop() :**

```cpp
// Après calcul next_mm :
static const float Z_MIN = 0.0f;   // Limite basse (sécurité collision)
static const float Z_MAX = 100.0f; // Limite haute (course max)

next_mm = constrain(next_mm, Z_MIN, Z_MAX);

if (next_mm != current_mm) {
  dxlGotoMm(i, next_mm);
}
```

## Voir aussi

- [00_OVERVIEW.md](00_OVERVIEW.md) — Vue générale du projet
- [05_OPENRB150.md](05_OPENRB150.md) — Détails firmware OpenRB
- [06_COMMUNICATION_PROTOCOL.md](06_COMMUNICATION_PROTOCOL.md) — Protocole série
- [14_HARDWARE_REFERENCE.md](14_HARDWARE_REFERENCE.md) — Schémas, pins, timing
- [15_CALIBRATION_FORCE.md](15_CALIBRATION_FORCE.md) — Calibration force (ADC → N)
- [`firmware/OPENRB150/src/main.cpp`](../firmware/OPENRB150/src/main.cpp) — Code source firmware
