# Banc de mesure de force — Compilation technique

> Document de référence (2026-06-10). Matériel : **LoLin NodeMCU v3 (ESP8266)**,
> projet PlatformIO `machine_final/ESP8266/` (env `nodemcuv2`).
> Note débit : le burst brut (8192 samples CSV ≈ 41 Ko) ne passe pas @ 4800 bauds
> (~85 s/burst). Implémentation retenue : l'OpenRB calcule le résumé (moyenne des
> X max + delta de recalage) et n'envoie que `PEAK:<...>` à chaque tour ; le burst
> complet (décimé ×16) n'est envoyé que sur demande `GETBURST` pour le debug.

## Sommaire
1. Architecture système
2. Câblage DM542T ↔ OpenRB-150
3. Pilotage stepper — fréquence fixe
4. Compteur de position et trigger au point bas
5. Timer hardware TC3 (SAMD21)
6. ADC freerun + DMA — burst d'acquisition
7. Centrage du burst sur le point bas
8. Dimensionnement mémoire et limites
9. Protocole série OpenRB-150 ↔ ESP8266
10. Logique de contrôle ESP8266
11. Interface utilisateur (USB / WebSocket)
12. Code complet intégré
13. Recalage des pas perdus par retour capteur de force

---
## 1. Architecture système
```
┌─────────────────────────────────────────────────────┐
│                   UTILISATEUR                       │
│            (navigateur / serial monitor)            │
│                                                     │
│   WebSocket (WiFi)  ou  USB Serial : "TARGET:2.5"  │
└──────────────────────┬──────────────────────────────┘
                       │
                       ▼
┌──────────────────────────────────────────────────────┐
│                    ESP8266 (cerveau)                    │
│                                                      │
│  - Reçoit les bursts ADC via série                   │
│  - Extrait les X valeurs max                         │
│  - Compare à la target de force (N)                  │
│  - Envoie commande UP/DN/HOLD à l'OpenRB-150        │
│  - Expose WebSocket + USB pour l'utilisateur         │
└──────────────────────┬───────────────────────────────┘
                       │ UART (série)
                       ▼
┌──────────────────────────────────────────────────────┐
│               OpenRB-150 (exécutant)                 │
│               MCU : SAMD21G18A                       │
│                                                      │
│  - Timer TC3 → pulse stepper DM542T                  │
│  - Compteur de position (steps dans la révolution)   │
│  - ADC freerun + DMA → burst de mesures              │
│  - Pilote Dynamixel (course verticale table)         │
│  - Envoie les bursts bruts à l'ESP8266                 │
│  - Reçoit et exécute UP/DN/HOLD                      │
└───────┬──────────────────────────┬───────────────────┘
        │                          │
        ▼                          ▼
┌───────────────┐        ┌──────────────────┐
│   DM542T      │        │   Dynamixel(s)   │
│   + Sanyo     │        │   (course vert.) │
│   Denki       │        └──────────────────┘
│   stepper     │
└───────────────┘
        │
        ▼
  Capteur de force (load cell 50N)
  → 4× INA125P (ampli instrumentation)
  → RC 1 Hz low-pass
  → ADC SAMD21 (pin A0)
```
Principe : l'OpenRB-150 est un pur exécutant — elle tourne le moteur, acquiert les données, et bouge les Dynamixel. Toute la logique de contrôle (comparaison à la target, décision de mouvement) est dans l'ESP8266.

---
## 2. Câblage DM542T ↔ OpenRB-150
### Signaux du DM542T
Le DM542T possède 6 bornes signal organisées en 3 paires :

| Paire   | Borne + | Borne - | Fonction                         |
|---------|---------|---------|----------------------------------|
| PUL     | PUL+    | PUL-    | Impulsion step (1 pulse = 1 µstep) |
| DIR     | DIR+    | DIR-    | Sens de rotation                 |
| ENA     | ENA+    | ENA-    | Enable driver (couple moteur)    |

### Mode anode commune (recommandé pour MCU 3.3V)
L'OpenRB-150 sort du 3.3V. Les optocoupleurs internes du DM542T fonctionnent mieux en anode commune : le 5V fournit la tension, le MCU "sink" le courant vers GND.
```
5V (OpenRB-150) ──┬──── PUL+
                   ├──── DIR+
                   └──── ENA+
Pin 6 (MCU) ──────────── PUL-    (signal dynamique : pulse)
Pin 7 (MCU) ──────────── DIR-    (fixe : sens de rotation)
Pin 8 (MCU) ──────────── ENA-    (fixe : enable)
GND (OpenRB-150) ──────── GND commun
```
### Logique des signaux (anode commune = actif bas)
| Signal | LOW              | HIGH               |
|--------|------------------|--------------------|
| DIR    | Sens horaire     | Sens anti-horaire  |
| ENA    | Driver activé    | Driver désactivé   |
| PUL    | Front montant → step | —               |

### Point de mesure oscilloscope
Sonde sur **PUL-**, masse sur **GND** de l'OpenRB-150. Signal carré inversé (actif bas) à la fréquence de pulse.

---
## 3. Pilotage stepper — fréquence fixe
### Approche simplifiée : `tone()`
Pour une fréquence constante sans comptage de position :
```cpp
#define PUL_PIN  6
#define DIR_PIN  7
#define ENA_PIN  8
float F = 500.0;  // fréquence de pulse en Hz
void setup() {
  pinMode(PUL_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENA_PIN, OUTPUT);
  digitalWrite(DIR_PIN, LOW);   // sens
  digitalWrite(ENA_PIN, LOW);   // enable
  tone(PUL_PIN, F);             // signal carré hardware
}
void loop() {}  // rien, tone() tourne en fond
```
- Arrêt : `noTone(PUL_PIN)`
- Changement : `tone(PUL_PIN, nouvelle_freq)`
- **Limitation** : pas de comptage de steps → pas de position dans la révolution

### Fallback bloquant (si tone() incompatible)
```cpp
void loop() {
  digitalWrite(PUL_PIN, HIGH);
  delayMicroseconds(1000000 / (2 * F));
  digitalWrite(PUL_PIN, LOW);
  delayMicroseconds(1000000 / (2 * F));
}
```
CPU 100% occupé — inutilisable avec acquisition en parallèle.

---
## 4. Compteur de position et trigger au point bas
### Relation fréquence de rotation / fréquence de pulse
```
F_pulse = F_rotation × STEPS_PER_REV
```
| Paramètre      | Variable        | Exemple          |
|-----------------|-----------------|------------------|
| Fréquence rotation | F_rotation   | 10 Hz (max)      |
| Steps par tour  | STEPS_PER_REV   | 3200 (config DIP)|
| Fréquence pulse | F_pulse         | 32 000 Hz        |
| Durée d'un tour | 1 / F_rotation  | 100 ms           |

Le DM542T accepte jusqu'à ~200 kHz en entrée pulse. À F_rotation = 10 Hz, F_pulse = 32 kHz → largement dans les specs.

### Principe du compteur
Le timer ISR incrémente `stepCount` à chaque front montant. Quand `stepCount` atteint `STEPS_PER_REV`, on est revenu au point bas → reset à 0 et déclenchement du burst.

---
## 5. Timer hardware TC3 (SAMD21)
Le SAMD21G18A (MCU de l'OpenRB-150) possède plusieurs timers 16 bits. On utilise TC3 pour générer les pulses stepper en interruption.

### Configuration
- Clock source : GCLK0 = 48 MHz
- Prescaler : /64 → tick à 750 kHz
- Mode : MFRQ (match frequency) — compare sur CC[0]
- Valeur compare : `750000 / (2 × F_rotation × STEPS_PER_REV) - 1`

On configure le timer à **2× la fréquence de pulse** car on toggle le pin à chaque interruption (2 toggles = 1 cycle complet = 1 step).

### Calcul de la valeur compare
```
timer_freq_hz = 2 × F_pulse = 2 × F_rotation × STEPS_PER_REV
compare = (750000 / timer_freq_hz) - 1
```
Pour F_rotation = 10 Hz :
```
timer_freq_hz = 2 × 10 × 3200 = 64 000 Hz
compare = (750000 / 64000) - 1 ≈ 10.7 → 10
```
### Priorité d'interruption
TC3 (stepper) : priorité 0 (max) — le jitter sur les pulses cause des vibrations moteur.
DMAC (ADC) : priorité 1 — peut tolérer quelques µs de latence.

---
## 6. ADC freerun + DMA — burst d'acquisition
### Pourquoi pas analogRead()
| Méthode         | Débit          | CPU occupé | Burst 8192 samples |
|-----------------|----------------|------------|---------------------|
| analogRead()    | ~25 kSPS       | 100%       | ~328 ms             |
| Freerun + DMA   | ~87 kSPS (×4 avg) | 0%      | ~94 ms              |

Avec analogRead(), le CPU est bloqué pendant le burst et ne peut pas maintenir les pulses stepper. Avec freerun + DMA, le hardware fait tout seul.

### ADC freerun
L'ADC est configuré en mode freerun : dès qu'une conversion est terminée, il en relance une automatiquement sans intervention du CPU.

Configuration :
- Clock : GCLK0 48 MHz, prescaler /32 → 1.5 MHz ADC clock
- Résolution : 12 bits (0–4095)
- Référence : VDDANA/2 avec gain 1/2
- Averaging hardware : 4 samples → ~87 kSPS effectif, moins de bruit
- Input : A0 (AIN0) = sortie INA125P après filtre RC 1 Hz

### DMA (Direct Memory Access)
Le contrôleur DMAC du SAMD21 copie chaque résultat ADC directement en RAM via un descripteur :
- Source : registre `ADC->RESULT` (16 bits, adresse fixe)
- Destination : `burstBuffer[]` (adresse incrémentée à chaque beat)
- Beat size : HWORD (16 bits)
- Nombre de beats : BURST_SIZE
- Fin de transfert : interruption TCMPL → `burstReady = true`

Le descripteur DMA doit être aligné sur 16 octets (contrainte hardware SAMD21). L'adresse destination pointe vers la **fin** du buffer (convention SAMD21 DMA : le pointeur est post-incrémenté).

### Séquence d'un burst
1. Timer ISR détecte le step de trigger → appelle `startBurst()`
2. `startBurst()` arme le descripteur DMA et active l'ADC
3. L'ADC convertit en boucle, le DMA range les résultats
4. Après BURST_SIZE conversions, le DMA coupe l'ADC et lève `burstReady`
5. `loop()` envoie les données sur Serial

Pendant toute la séquence (étapes 2–4), le CPU est libre et le timer TC3 continue à générer les pulses stepper.

---
## 7. Centrage du burst sur le point bas
### Problème
Si on déclenche le burst à step 0, le point bas est le **premier** échantillon. On veut qu'il soit au **centre** du burst pour capturer la descente et la remontée autour du minimum de force.

### Solution
Déclencher le burst en avance de T_burst / 2 :
```
T_burst     = BURST_SIZE / SAMPLE_RATE
offset_time = T_burst / 2
offset_steps = F_pulse × offset_time
trigger_step = STEPS_PER_REV - offset_steps
```
### Calcul numérique (F_rotation = 10 Hz, 8192 samples, 87 kSPS)
```
T_burst      = 8192 / 87000          = 94.2 ms
offset_time  = 94.2 / 2              = 47.1 ms
offset_steps = 32000 × 0.0471        = 1507 steps
trigger_step = 3200 - 1507           = 1693
```
Le burst démarre au step 1693. Le sample `burstBuffer[BURST_SIZE/2]` (index 4096) correspond au step 0 = point bas exact.

### Code
```cpp
const float SAMPLE_RATE = 87000.0;
const int OFFSET_STEPS = (int)(F_pulse * (BURST_SIZE / (2.0 * SAMPLE_RATE)));
const int TRIGGER_STEP = STEPS_PER_REV - OFFSET_STEPS;
// Dans TC3_Handler :
if (stepCount == TRIGGER_STEP && !burstArmed && !burstReady) {
  startBurst();
}
```

---
## 8. Dimensionnement mémoire et limites
### RAM disponible
SAMD21G18A : **32 768 octets** de SRAM.

| Poste                        | Occupation |
|------------------------------|------------|
| Arduino core (.data/.bss)    | ~2.5 KB    |
| Stack                        | ~2 KB      |
| Serial TX + RX buffers       | 512 octets |
| DMA descriptors (alignés)    | 32 octets  |
| Variables globales sketch     | ~50 octets |
| Marge sécurité               | ~1 KB      |
| **Total overhead**           | **~6 KB**  |
| **Disponible pour buffer**   | **~26 KB** |

### Taille max du burst
Chaque sample = `uint16_t` = 2 octets → **~13 000 samples max** (plafond sûr : **12 000**).

### Tableau de dimensionnement (à 87 kSPS, F_rotation = 10 Hz, 1 tour = 100 ms)
| BURST_SIZE | RAM     | Durée burst | % du tour | Marge avant prochain tour |
|------------|---------|-------------|-----------|---------------------------|
| 512        | 1 KB    | 6 ms        | 6%        | 94 ms                     |
| 2 048      | 4 KB    | 24 ms       | 24%       | 76 ms                     |
| 4 096      | 8 KB    | 47 ms       | 47%       | 53 ms                     |
| 8 192      | 16 KB   | 94 ms       | 94%       | 6 ms                      |
| 8 700      | 17 KB   | 100 ms      | 100%      | 0 ms (limite absolue)     |
| 12 000     | 24 KB   | 138 ms      | >100%     | **DÉBORDE sur tour suivant** |

**Sweet spot : 8 192 samples** — couvre 94% du tour, tient en RAM, et laisse 6 ms de marge.

---
## 9. Protocole série OpenRB-150 ↔ ESP8266
### Format texte lisible (monitorable sur terminal)
#### OpenRB-150 → ESP8266 : données de burst
```
BURST:<sample_0>,<sample_1>,...,<sample_N>\n
```
Chaque valeur est un entier 12 bits (0–4095), séparé par des virgules. Exemple :
```
BURST:2048,2051,2047,2053,2049,...,2046\n
```
#### ESP8266 → OpenRB-150 : commandes de mouvement
```
UP:<steps>\n      Monter la table de <steps> pas Dynamixel
DN:<steps>\n      Descendre la table de <steps> pas Dynamixel
HOLD\n            Ne pas bouger (force dans la cible)
```
#### Utilisateur → ESP8266 : consignes
```
TARGET:<force_N>\n    Définir la target de force en Newtons
START\n               Démarrer l'acquisition cyclique
STOP\n                Arrêter
```
Ces commandes arrivent indifféremment par USB Serial ou WebSocket — même parseur.

---
## 10. Logique de contrôle ESP8266
### Traitement du burst
1. Réception de la ligne `BURST:...`
2. Parsing des valeurs dans un tableau
3. Tri partiel : extraction des X plus grandes valeurs
4. Calcul de la moyenne des X max → conversion ADC → Newtons
5. Comparaison à la target avec deadband
6. Envoi de la commande appropriée

### Conversion ADC → Force
```
V_adc = (sample / 4095) × V_ref
V_force = V_adc / Gain_INA125P
Force_N = V_force / (Sensibilité × V_excitation)
```
Avec le capteur 50N (1.47230 mV/V, plage utile 4.5N) :
- Sensibilité = 1.47230 mV/V
- V_excitation = tension d'alimentation du pont

### Algorithme de décision
```cpp
float error = forceTarget - mean_max_force;
float deadband = 0.01;  // ±10 mN (target précision ±1 mN)
if (error > deadband) {
  // Force mesurée trop faible → descendre la table (rapprocher)
  Serial2.printf("DN:%d\n", stepsFromError(error));
} else if (error < -deadband) {
  // Force mesurée trop forte → monter la table (éloigner)
  Serial2.printf("UP:%d\n", stepsFromError(-error));
} else {
  Serial2.println("HOLD");
}
```

---
## 11. Interface utilisateur (USB / WebSocket)
L'ESP8266 expose deux interfaces parallèles :

| Interface       | Usage                        | Connexion           |
|-----------------|------------------------------|---------------------|
| USB Serial      | Debug, labo, terminal        | Câble USB direct    |
| WebSocket (WiFi)| Pilotage à distance, browser | Réseau local WiFi   |

Les deux partagent le même parseur de commandes. L'ESP8266 traite dans sa loop :
```cpp
void loop() {
  // Commandes utilisateur (target, start, stop)
  if (Serial.available())    parseUserCommand(Serial.readStringUntil('\n'));
  if (ws.hasMessage())       parseUserCommand(ws.readMessage());
  // Données du banc (bursts ADC)
  if (Serial2.available())   parseBurstData(Serial2.readStringUntil('\n'));
}
```

---
## 12. Code complet intégré — OpenRB-150
```cpp
// """
// Banc de mesure de force — OpenRB-150 (SAMD21G18A)
// Stepper DM542T (timer TC3) + ADC freerun DMA burst centré sur point bas
// """
// --- Pins ------------------------------------------------------------------
#define PUL_PIN    6
#define DIR_PIN    7
#define ENA_PIN    8
#define ADC_PIN    A0
// --- Paramètres moteur -----------------------------------------------------
const float F_ROTATION    = 10.0;                          // Hz (tours/s) — max
const int   STEPS_PER_REV = 3200;                          // µsteps/tour (config DIP DM542T)
const float F_PULSE       = F_ROTATION * STEPS_PER_REV;    // = 32 000 Hz
// --- Paramètres burst ------------------------------------------------------
const int   BURST_SIZE    = 8192;
const float SAMPLE_RATE   = 87000.0;                       // Hz (freerun + avg ×4)
// --- Trigger anticipé : centrage sur point bas -----------------------------
const int OFFSET_STEPS = (int)(F_PULSE * (BURST_SIZE / (2.0 * SAMPLE_RATE)));
const int TRIGGER_STEP = STEPS_PER_REV - OFFSET_STEPS;
// --- Buffers ---------------------------------------------------------------
volatile uint16_t burstBuffer[BURST_SIZE] __attribute__((aligned(4)));
volatile bool     burstReady = false;
volatile bool     burstArmed = false;
// --- Stepper state ---------------------------------------------------------
volatile int  stepCount = 0;
volatile bool pulState  = false;
// --- DMA descriptors (alignés 16 octets, imposé par DMAC SAMD21) ----------
typedef struct {
  uint16_t btctrl;
  uint16_t btcnt;
  uint32_t srcaddr;
  uint32_t dstaddr;
  uint32_t descaddr;
} DmacDescriptor_t;
__attribute__((aligned(16))) static DmacDescriptor_t dmacDescriptors[1];
__attribute__((aligned(16))) static DmacDescriptor_t dmacWriteback[1];
// ===========================================================================
// ADC freerun
// ===========================================================================
void setupADC() {
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN |
                       GCLK_CLKCTRL_GEN_GCLK0 |
                       GCLK_CLKCTRL_ID_ADC;
  while (GCLK->STATUS.bit.SYNCBUSY);
  ADC->CTRLA.bit.ENABLE = 0;
  while (ADC->STATUS.bit.SYNCBUSY);
  ADC->CTRLA.bit.SWRST = 1;
  while (ADC->CTRLA.bit.SWRST);
  ADC->REFCTRL.reg = ADC_REFCTRL_REFSEL_INTVCC1;
  ADC->CTRLB.reg   = ADC_CTRLB_PRESCALER_DIV32 |
                      ADC_CTRLB_RESSEL_12BIT |
                      ADC_CTRLB_FREERUN;
  while (ADC->STATUS.bit.SYNCBUSY);
  ADC->INPUTCTRL.reg = ADC_INPUTCTRL_MUXNEG_GND |
                        ADC_INPUTCTRL_MUXPOS_PIN0 |
                        ADC_INPUTCTRL_GAIN_DIV2;
  while (ADC->STATUS.bit.SYNCBUSY);
  ADC->AVGCTRL.reg = ADC_AVGCTRL_SAMPLENUM_4 |
                      ADC_AVGCTRL_ADJRES(2);
  while (ADC->STATUS.bit.SYNCBUSY);
  ADC->SAMPCTRL.reg = ADC_SAMPCTRL_SAMPLEN(4);
  while (ADC->STATUS.bit.SYNCBUSY);
}
// ===========================================================================
// DMA — canal 0, trigger = ADC RESRDY
// ===========================================================================
void setupDMA() {
  PM->AHBMASK.bit.DMAC_  = 1;
  PM->APBBMASK.bit.DMAC_ = 1;
  DMAC->CTRL.bit.DMAENABLE = 0;
  DMAC->CTRL.bit.SWRST     = 1;
  DMAC->BASEADDR.reg = (uint32_t)dmacDescriptors;
  DMAC->WRBADDR.reg  = (uint32_t)dmacWriteback;
  DMAC->CTRL.reg     = DMAC_CTRL_DMAENABLE | DMAC_CTRL_LVLEN(0xF);
  DMAC->CHID.reg     = 0;
  DMAC->CHCTRLB.reg  = DMAC_CHCTRLB_TRIGSRC(ADC_DMAC_ID_RESRDY) |
                        DMAC_CHCTRLB_TRIGACT_BEAT |
                        DMAC_CHCTRLB_LVL(0);
  DMAC->CHINTENSET.reg = DMAC_CHINTENSET_TCMPL;
  NVIC_EnableIRQ(DMAC_IRQn);
  NVIC_SetPriority(DMAC_IRQn, 1);
}
void armDMABurst() {
  DMAC->CHID.reg            = 0;
  DMAC->CHCTRLA.bit.ENABLE  = 0;
  dmacDescriptors[0].btctrl  = DMAC_BTCTRL_VALID |
                                DMAC_BTCTRL_BEATSIZE_HWORD |
                                DMAC_BTCTRL_DSTINC |
                                DMAC_BTCTRL_BLOCKACT_INT;
  dmacDescriptors[0].btcnt   = BURST_SIZE;
  dmacDescriptors[0].srcaddr = (uint32_t)&ADC->RESULT.reg;
  dmacDescriptors[0].dstaddr = (uint32_t)burstBuffer + BURST_SIZE * sizeof(uint16_t);
  dmacDescriptors[0].descaddr = 0;
  DMAC->CHID.reg            = 0;
  DMAC->CHCTRLA.bit.ENABLE  = 1;
}
// ===========================================================================
// DMA ISR — burst terminé
// ===========================================================================
void DMAC_Handler() {
  DMAC->CHID.reg = 0;
  if (DMAC->CHINTFLAG.bit.TCMPL) {
    DMAC->CHINTFLAG.bit.TCMPL = 1;
    ADC->CTRLA.bit.ENABLE = 0;
    while (ADC->STATUS.bit.SYNCBUSY);
    burstReady = true;
    burstArmed = false;
  }
}
// ===========================================================================
// Lancer un burst
// ===========================================================================
void startBurst() {
  burstReady = false;
  burstArmed = true;
  armDMABurst();
  ADC->CTRLA.bit.ENABLE = 1;
  while (ADC->STATUS.bit.SYNCBUSY);
  ADC->SWTRIG.bit.START = 1;
  while (ADC->STATUS.bit.SYNCBUSY);
}
// ===========================================================================
// Timer TC3 — pulse stepper + compteur position
// ===========================================================================
void TC3_Handler() {
  if (TC3->COUNT16.INTFLAG.bit.MC0) {
    TC3->COUNT16.INTFLAG.bit.MC0 = 1;
    pulState = !pulState;
    digitalWrite(PUL_PIN, pulState);
    if (pulState) {  // front montant = 1 step complet
      stepCount++;
      if (stepCount >= STEPS_PER_REV) {
        stepCount = 0;
      }
      // Trigger anticipé pour centrer le burst sur le point bas
      if (stepCount == TRIGGER_STEP && !burstArmed && !burstReady) {
        startBurst();
      }
    }
  }
}
void setupTimer(float pulseFreq) {
  float timerFreq = pulseFreq * 2.0;
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN |
                       GCLK_CLKCTRL_GEN_GCLK0 |
                       GCLK_CLKCTRL_ID_TCC2_TC3;
  while (GCLK->STATUS.bit.SYNCBUSY);
  TC3->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
  while (TC3->COUNT16.STATUS.bit.SYNCBUSY);
  TC3->COUNT16.CTRLA.reg = TC_CTRLA_MODE_COUNT16 |
                            TC_CTRLA_PRESCALER_DIV64 |
                            TC_CTRLA_WAVEGEN_MFRQ;
  uint16_t compare = (uint16_t)(750000.0 / timerFreq) - 1;
  TC3->COUNT16.CC[0].reg = compare;
  while (TC3->COUNT16.STATUS.bit.SYNCBUSY);
  TC3->COUNT16.INTENSET.reg = TC_INTENSET_MC0;
  NVIC_EnableIRQ(TC3_IRQn);
  NVIC_SetPriority(TC3_IRQn, 0);  // priorité max
  TC3->COUNT16.CTRLA.bit.ENABLE = 1;
  while (TC3->COUNT16.STATUS.bit.SYNCBUSY);
}
// ===========================================================================
// Réception commandes depuis ESP8266
// ===========================================================================
void parseCommand(String cmd) {
  cmd.trim();
  if (cmd.startsWith("UP:")) {
    int steps = cmd.substring(3).toInt();
    // TODO: déplacer Dynamixel de +steps
  } else if (cmd.startsWith("DN:")) {
    int steps = cmd.substring(3).toInt();
    // TODO: déplacer Dynamixel de -steps
  } else if (cmd == "HOLD") {
    // ne rien faire
  }
}
// ===========================================================================
// Setup
// ===========================================================================
void setup() {
  pinMode(PUL_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENA_PIN, OUTPUT);
  digitalWrite(DIR_PIN, LOW);
  digitalWrite(ENA_PIN, LOW);
  Serial.begin(115200);   // vers ESP8266
  analogReadResolution(12);
  setupADC();
  setupDMA();
  setupTimer(F_PULSE);
  Serial.println("READY");
}
// ===========================================================================
// Loop
// ===========================================================================
void loop() {
  // --- Envoi du burst quand prêt -------------------------------------------
  if (burstReady) {
    burstReady = false;
    Serial.print("BURST:");
    for (int i = 0; i < BURST_SIZE; i++) {
      Serial.print(burstBuffer[i]);
      if (i < BURST_SIZE - 1) Serial.print(',');
    }
    Serial.println();
  }
  // --- Réception commandes ESP8266 -------------------------------------------
  if (Serial.available()) {
    parseCommand(Serial.readStringUntil('\n'));
  }
}
```

---
## Annexe — Résumé des constantes clés
| Constante         | Valeur       | Unité     | Source / Remarque                     |
|-------------------|-------------|-----------|---------------------------------------|
| F_ROTATION max    | 10          | Hz        | Spec banc                             |
| STEPS_PER_REV     | 3200        | steps     | Config DIP DM542T (µstep 1/16)       |
| F_PULSE max       | 32 000      | Hz        | F_ROTATION × STEPS_PER_REV           |
| DM542T pulse max  | ~200 000    | Hz        | Datasheet DM542T                      |
| BURST_SIZE        | 8 192       | samples   | Sweet spot RAM/couverture             |
| SAMPLE_RATE       | 87 000      | SPS       | ADC freerun, avg ×4, prescaler /32   |
| Durée burst       | 94          | ms        | BURST_SIZE / SAMPLE_RATE              |
| Durée tour (10 Hz)| 100         | ms        | 1 / F_ROTATION                        |
| OFFSET_STEPS      | 1 507       | steps     | F_PULSE × BURST_SIZE / (2 × SAMPLE_RATE) |
| TRIGGER_STEP      | 1 693       | step      | STEPS_PER_REV - OFFSET_STEPS          |
| RAM buffer        | 16 384      | octets    | BURST_SIZE × 2                        |
| RAM dispo SAMD21  | ~26 000     | octets    | 32 768 - overhead                     |
| Max burst (RAM)   | ~13 000     | samples   | RAM dispo / 2                         |
| Max burst (temps) | 8 700       | samples   | SAMPLE_RATE × durée tour              |
| Load cell         | 50 N        | N         | Plage utile : 4.5 N                  |
| Sensibilité       | 1.47230     | mV/V      | Datasheet capteur                     |
| Précision cible   | ±1          | mN        | Spec banc                             |
| Deadband contrôle | ±10         | mN        | Marge pour éviter oscillation         |
| Résolution ADC    | 12          | bits      | 0–4095                                |
| TC3 prescaler     | /64         | —         | 48 MHz / 64 = 750 kHz tick           |
| TC3 priorité IRQ  | 0           | —         | Max (stepper critique)                |
| DMAC priorité IRQ | 1           | —         | Secondaire (ADC tolère latence)       |

---
## 13. Recalage des pas perdus par retour capteur de force
### Problème
Les moteurs pas-à-pas peuvent perdre des pas (charge mécanique trop forte, accélération trop brusque, vibrations). Sans encodeur, le compteur `stepCount` dérive progressivement par rapport à la position physique réelle du mécanisme. Sur un banc de mesure de force, ce décalage fausse le centrage du burst sur le point bas.

### Principe
On sait que la force est maximale au point bas physique (compression maximale sur le capteur). Le burst est censé être centré sur ce point bas, donc le pic de force devrait tomber à l'index `BURST_SIZE / 2` du buffer. Si le pic est décalé, c'est que `stepCount` a dérivé — le décalage en samples donne directement l'erreur en steps.

Le capteur de force fait office de feedback de position : pas besoin d'encodeur.

### Calcul du recalage
1. Trouver l'index du sample max dans le burst (= point bas physique réel)
2. Comparer à l'index attendu (`BURST_SIZE / 2`)
3. Convertir le décalage en samples vers un décalage en steps
4. Corriger `stepCount`
```
index_pic_réel    = argmax(burstBuffer)
index_pic_attendu = BURST_SIZE / 2
delta_samples     = index_pic_réel - index_pic_attendu
delta_steps       = delta_samples × (F_PULSE / SAMPLE_RATE)
stepCount        += delta_steps
```
Avec les valeurs du banc (F_PULSE = 32 000 Hz, SAMPLE_RATE = 87 000 SPS) :
```
ratio = F_PULSE / SAMPLE_RATE = 32000 / 87000 ≈ 0.368 steps/sample
```
Un décalage de 10 samples dans le burst correspond à ~3.7 steps perdus.

### Implémentation côté OpenRB-150
Le recalage se fait dans `loop()`, juste après que le burst est prêt et avant l'envoi à l'ESP8266. C'est un simple argmax O(n) sur le buffer déjà en RAM — coût négligeable.
```cpp
if (burstReady) {
  // --- Recalage sur pic de force -----------------------------------------
  int iMax = 0;
  for (int i = 1; i < BURST_SIZE; i++) {
    if (burstBuffer[i] > burstBuffer[iMax]) iMax = i;
  }
  int delta = iMax - (BURST_SIZE / 2);
  stepCount += (int)(delta * (F_PULSE / SAMPLE_RATE));
  // --- Envoi du burst à l'ESP8266 ------------------------------------------
  burstReady = false;
  Serial.print("BURST:");
  for (int i = 0; i < BURST_SIZE; i++) {
    Serial.print(burstBuffer[i]);
    if (i < BURST_SIZE - 1) Serial.print(',');
  }
  Serial.println();
}
```

### Pourquoi côté OpenRB et pas ESP8266
L'OpenRB-150 possède `stepCount` et le buffer brut en RAM. Le recalage est une opération locale (argmax + addition) qui n'a pas besoin de transiter par la liaison série. L'ESP8266 n'a même pas besoin de savoir que le recalage a eu lieu — la séparation des responsabilités est préservée.
