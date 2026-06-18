/**
 * Banc de mesure de force — OpenRB-150 (SAMD21G18A)  —  PHASE 2, ÉTAPE 1
 *
 * Rôle : exécutant. Reçoit les commandes (protocole ligne) depuis l'ESP8266
 * via Serial3, pilote le stepper (DM542T) et les 4 Dynamixel (tables),
 * lit les 4 cellules de force (INA125 -> ADC), et renvoie l'état.
 *
 * ÉTAPE 1 (ce fichier) : AUCUN mouvement autonome.
 *   - SET_FREQ / START / STOP   -> oscillation stepper (commandée)
 *   - GOTO:<table>:<mm>         -> position Dynamixel (commandée)
 *   - SET_FORCE[:cell]:<N>      -> mémorise la consigne (PAS de descente auto)
 *   - GET_STATUS               -> STATE / FREQ / POSITION / FORCE / SLAVE
 * La boucle fermée de force (descente 1 pas/cycle, remontée rapide si
 * dépassement, garde-fou 49 N) sera ajoutée à l'ÉTAPE 2 — voir les TODO.
 *
 * ===== CÂBLAGE =====
 * Stepper DM542T (anode commune, actif bas) :
 *   D6 = PUL (step, généré par Timer TC3)   D7 = DIR   D8 = ENA
 * Cellules de force (4x INA125 -> RC 1 Hz) : A1, A2, A3, A4
 * Relais sélection résistance (30/90 ohm)  : D4, D5  (rôle à préciser, non utilisé ici)
 * Bus Dynamixel                            : Serial1 (interne OpenRB-150)
 * Lien vers l'ESP8266                      : Serial3  (RX=D13, TX=D14)
 *      OpenRB D13(RX) <- ESP GPIO12(D6, TX) | OpenRB D14(TX) -> ESP GPIO14(D5, RX)
 */

#include <Arduino.h>
#include <Dynamixel2Arduino.h>

// ===== Configuration =======================================================

// --- Lien série vers l'ESP8266 ---
#define LINK            Serial3
// ÉTAPE 2 FIX: réduit de 19200 → 9600 pour stabilité (SoftwareSerial + WiFi interference)
// Trade-off: latence +2x mais zéro déconnexions aléatoires
#define LINK_BAUD       9600       // Réduit pour stabilité SoftwareSerial sur ESP8266

// --- Streaming statut (liaison permanente) ---
// On émet le burst de statut tout seul à 10 Hz, sans attendre de GET_STATUS.
// L'ESP cache le dernier burst -> /api/status devient instantané (pas d'A/R série).
#define STREAM_PERIOD_MS 100

// --- Stepper DM542T ---
static const uint8_t PUL_PIN = 6;
static const uint8_t DIR_PIN = 7;
static const uint8_t ENA_PIN = 8;
static const int     STEPS_PER_REV = 3200;   // config DIP DM542T (µstep 1/16)
static const float   F_ROTATION_MAX = 10.0f; // Hz (spec banc)

// --- Cellules de force (INA125 -> ADC) ---
static const uint8_t FORCE_PINS[4] = { A1, A2, A3, A4 };
static const uint8_t FORCE_BURST   = 10;     // échantillons moyennés / mesure (anti-bruit)
static const float   ADC_VREF = 3300.0f;     // Tension de référence ADC (mV) — SAMD21
static const float   ADC_RESOLUTION_STEPS = 4096.0f; // 12-bit ADC (0 → 4095)
// Phase 1 : envoyer mV bruts au backend (calibration là-bas)
// Phase 2 : FORCE_GAIN/OFFSET seront reçus du backend via protocole
static float  FORCE_GAIN[4]   = { 1.0f, 1.0f, 1.0f, 1.0f };  // N par count ADC (Phase 2)
static float  FORCE_OFFSET[4] = { 0.0f, 0.0f, 0.0f, 0.0f };  // count ADC à vide (Phase 2)
static const float FORCE_MAX_N = 49.0f;      // garde-fou capteur (cellule 50 N) — ÉTAPE 2

// === ÉTAPE 2 : BOUCLE FERMÉE DE FORCE ===

// Fenêtre d'acquisition (plage du cycle stepper où on prend les mesures)
// Les 4 capteurs touchent ensemble au bas du cycle stepper → même moment (même mobile)
// Zone contact : [3050, 3200] = derniers 5% du cycle (0→3200)
static const uint16_t FORCE_WINDOW_START = 3050;  // ~95% du cycle
static const uint16_t FORCE_WINDOW_END = 3200;    // 100% (point bas exact)
static uint16_t g_forceWindowStart = FORCE_WINDOW_START;
static uint16_t g_forceWindowEnd = FORCE_WINDOW_END;

// Seuil de contact (force doit dépasser ce seuil pour confirmer contact)
static const float FORCE_CONTACT_THRESHOLD = 0.5f;  // 500 mV

// Stratégie 3-phases de descente/remontée
static const float FINE_TUNE_ZONE_MM = 20.0f;    // Au-dessus = descente rapide
static const float STEP_DOWN_FAST_MM = 1.0f;     // Pas rapide (phase 1)
static const float STEP_DOWN_SLOW_MM = 0.1f;     // Pas lent (phase 2, fine-tuning)
static const float FORCE_OVERSHOOT_THRESHOLD = 1.05f;  // 105% de cible (5% dépassement)
static const float STEP_UP_FAST_MM = 1.0f;       // Remontée rapide si > 5% dépassement
static const float STEP_UP_SLOW_MM = 0.2f;       // Remontée lente si ≤ 5% dépassement

// Position au boot après homing
static const float BOOT_POSITION_MM = 96.0f;     // Z haute

// Deadband force (tolérance "force OK")
static const float FORCE_DEADBAND = 0.01f;       // ±10 mV

// Timing boucle fermée
static const uint32_t FORCE_LOOP_INTERVAL_MS = 50;  // 20 Hz

// Peak tracking (max force du cycle actuel)
static float g_forcePeakCycle[4] = { 0, 0, 0, 0 };  // Force max durant fenêtre
static uint16_t g_forcePeakStepCountCycle = 0;      // g_stepCount du peak

// État de la boucle fermée
static uint32_t lastForceLoopMs = 0;
static bool lastInForceWindow = false;

// Phase de contrôle par table (pour logging/debug)
enum ForceControlPhase {
  PHASE_IDLE = 0,
  PHASE_DESCENT_FAST = 1,
  PHASE_DESCENT_SLOW = 2,
  PHASE_HOLDING = 3,
  PHASE_ASCENT_SLOW = 4,
  PHASE_ASCENT_FAST = 5
};
static uint8_t g_forcePhase[4] = { PHASE_IDLE, PHASE_IDLE, PHASE_IDLE, PHASE_IDLE };

// --- Dynamixel (tables) ---
#define DXL_SERIAL      Serial1
static const float    DXL_PROTOCOL = 2.0f;
static const uint32_t DXL_BAUD     = 57600;  // baud du bus (à adapter si besoin)
static const int      DXL_DIR_PIN  = -1;     // OpenRB-150 gère la direction en interne
static const uint8_t  DXL_SCAN_MAX = 20;     // on ping les IDs 1..DXL_SCAN_MAX
static const uint8_t  N_TABLES     = 4;
// Conversion mm <-> position Dynamixel (PLACEHOLDER — à calibrer) :
static const float    DXL_PER_MM   = 1.0f;

Dynamixel2Arduino dxl(DXL_SERIAL, DXL_DIR_PIN);

// ===== État machine ========================================================

enum class Mode { IDLE, HOMING, READY, RUNNING, ERROR };

static Mode     g_mode = Mode::IDLE;
static bool     g_homed = false;
static int32_t  g_speed = 100;          // %
static float    g_frequency = 0.8f;     // Hz (oscillation)
static float    g_forceTarget[4] = { 0, 0, 0, 0 };  // consignes (ÉTAPE 2)
static float    g_force[4] = { 0, 0, 0, 0 };        // mesures (N)

static uint8_t  g_dxlIds[4] = { 0, 0, 0, 0 };
static uint8_t  g_dxlCount = 0;

// ===== Stepper : Timer TC3 =================================================

volatile int  g_stepCount = 0;
volatile bool g_pulState  = false;
static bool   g_stepperRunning = false;

extern "C" void TC3_Handler() {
    if (TC3->COUNT16.INTFLAG.bit.MC0) {
        TC3->COUNT16.INTFLAG.bit.MC0 = 1;
        g_pulState = !g_pulState;
        digitalWrite(PUL_PIN, g_pulState);
        if (g_pulState) {                 // front montant = 1 step
            g_stepCount++;
            if (g_stepCount >= STEPS_PER_REV) g_stepCount = 0;
        }
    }
}

static void stepperTimerInit() {
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 |
                        GCLK_CLKCTRL_ID_TCC2_TC3;
    while (GCLK->STATUS.bit.SYNCBUSY);

    TC3->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
    while (TC3->COUNT16.STATUS.bit.SYNCBUSY);

    TC3->COUNT16.CTRLA.reg = TC_CTRLA_MODE_COUNT16 |
                             TC_CTRLA_PRESCALER_DIV64 |
                             TC_CTRLA_WAVEGEN_MFRQ;
    TC3->COUNT16.INTENSET.reg = TC_INTENSET_MC0;
    NVIC_EnableIRQ(TC3_IRQn);
    NVIC_SetPriority(TC3_IRQn, 0);        // priorité max (jitter -> vibrations)
}

// Règle la fréquence d'oscillation (Hz). <=0 : arrêt des pulses.
static void stepperSetFrequency(float fRot) {
    if (fRot <= 0.0f) {
        TC3->COUNT16.CTRLA.bit.ENABLE = 0;
        while (TC3->COUNT16.STATUS.bit.SYNCBUSY);
        g_stepperRunning = false;
        return;
    }
    if (fRot > F_ROTATION_MAX) fRot = F_ROTATION_MAX;

    float fPulse    = fRot * STEPS_PER_REV;
    float timerFreq = fPulse * 2.0f;                 // toggle 2x par cycle
    uint16_t compare = (uint16_t)(750000.0f / timerFreq) - 1;

    TC3->COUNT16.CTRLA.bit.ENABLE = 0;
    while (TC3->COUNT16.STATUS.bit.SYNCBUSY);
    TC3->COUNT16.CC[0].reg = compare;
    while (TC3->COUNT16.STATUS.bit.SYNCBUSY);
    TC3->COUNT16.CTRLA.bit.ENABLE = 1;
    while (TC3->COUNT16.STATUS.bit.SYNCBUSY);
    g_stepperRunning = true;
}

// ===== Force : lecture INA125 (moyenne d'un burst) =========================

// Phase 1 : envoie les mV bruts au backend (calibration là-bas)
static float readForcemV(uint8_t cell) {
    uint32_t acc = 0;
    for (uint8_t i = 0; i < FORCE_BURST; i++) acc += analogRead(FORCE_PINS[cell]);
    float counts = (float)acc / FORCE_BURST;
    // Convertir ADC counts -> mV
    // mV = (counts / 4096) × VREF_mV
    return (counts / ADC_RESOLUTION_STEPS) * ADC_VREF;
}

// Phase 2 : sera utilisé pour la boucle fermée locale (après réception calibration du backend)
static float readForceN(uint8_t cell) {
    uint32_t acc = 0;
    for (uint8_t i = 0; i < FORCE_BURST; i++) acc += analogRead(FORCE_PINS[cell]);
    float counts = (float)acc / FORCE_BURST;
    return (counts - FORCE_OFFSET[cell]) * FORCE_GAIN[cell];
}

static void readAllForces() {
    // Phase 1 : envoyer mV bruts
    for (uint8_t i = 0; i < N_TABLES; i++) g_force[i] = readForcemV(i);
}

// ===== Dynamixel ===========================================================

static void dxlScan() {
    g_dxlCount = 0;
    for (uint8_t id = 1; id <= DXL_SCAN_MAX && g_dxlCount < N_TABLES; id++) {
        if (dxl.ping(id)) {
            g_dxlIds[g_dxlCount++] = id;
        }
    }
    for (uint8_t i = 0; i < g_dxlCount; i++) {
        dxl.torqueOff(g_dxlIds[i]);
        dxl.setOperatingMode(g_dxlIds[i], OP_POSITION);
        dxl.torqueOn(g_dxlIds[i]);
    }
}

static float dxlPositionMm(uint8_t table) {
    if (table >= g_dxlCount) return 0.0f;
    float pos = dxl.getPresentPosition(g_dxlIds[table]);  // unités Dynamixel
    return pos / DXL_PER_MM;
}

static void dxlGotoMm(uint8_t table, float mm) {
    if (table >= g_dxlCount) return;
    dxl.setGoalPosition(g_dxlIds[table], mm * DXL_PER_MM);
}

// LED Dynamixel = feedback visuel du mode : RUNNING -> clignotement,
// tout autre mode (IDLE/READY/...) -> allumées fixe (statique).
// On n'écrit sur le bus qu'au toggle ou au changement de mode (pas de spam).
static void updateDxlLeds() {
    static const uint32_t BLINK_PERIOD_MS = 250;
    static uint32_t lastToggleMs = 0;
    static bool     blinkOn = false;
    static Mode     lastMode = Mode::ERROR;   // != au 1er passage -> force un refresh

    if (g_mode == Mode::RUNNING) {
        uint32_t now = millis();
        if (lastMode != Mode::RUNNING || now - lastToggleMs >= BLINK_PERIOD_MS) {
            lastToggleMs = now;
            blinkOn = !blinkOn;
            for (uint8_t i = 0; i < g_dxlCount; i++) {
                if (blinkOn) dxl.ledOn(g_dxlIds[i]);
                else         dxl.ledOff(g_dxlIds[i]);
            }
        }
        lastMode = Mode::RUNNING;
    } else if (lastMode != g_mode) {
        // Entrée dans un mode non-RUNNING : allumer fixe, une seule fois.
        for (uint8_t i = 0; i < g_dxlCount; i++) dxl.ledOn(g_dxlIds[i]);
        lastMode = g_mode;
        blinkOn  = true;
    }
}

// ===== ÉTAPE 2 : Boucle fermée de force ====================================

// Forward declarations
static void handleHardReset();

struct ForceControl {
    uint8_t phase;
    float step_mm;
};

static bool isInForceWindow() {
    if (g_forceWindowStart <= g_forceWindowEnd) {
        return g_stepCount >= g_forceWindowStart && g_stepCount <= g_forceWindowEnd;
    } else {
        return g_stepCount >= g_forceWindowStart || g_stepCount <= g_forceWindowEnd;
    }
}

static ForceControl getControlPhaseAndStep(uint8_t table_i, float current_mm, float target_force, float measured_force) {
    float error = target_force - measured_force;

    if (current_mm > FINE_TUNE_ZONE_MM && error > FORCE_DEADBAND) {
        return { PHASE_DESCENT_FAST, -STEP_DOWN_FAST_MM };
    }

    if (current_mm <= FINE_TUNE_ZONE_MM && error > FORCE_DEADBAND) {
        return { PHASE_DESCENT_SLOW, -STEP_DOWN_SLOW_MM };
    }

    if (error > -FORCE_DEADBAND && error < +FORCE_DEADBAND) {
        return { PHASE_HOLDING, 0.0f };
    }

    if (error < -FORCE_DEADBAND) {
        float overshoot_ratio = measured_force / target_force;
        if (overshoot_ratio > FORCE_OVERSHOOT_THRESHOLD) {
            return { PHASE_ASCENT_FAST, +STEP_UP_FAST_MM };
        } else {
            return { PHASE_ASCENT_SLOW, +STEP_UP_SLOW_MM };
        }
    }

    return { PHASE_IDLE, 0.0f };
}

static void updateForceLoop() {
    if (g_mode != Mode::RUNNING) return;

    bool nowInWindow = isInForceWindow();

    // PENDANT fenêtre : lire forces, tracker peaks, PAS de mouvements
    if (nowInWindow) {
        if (!lastInForceWindow) {
            for (uint8_t i = 0; i < N_TABLES; i++) {
                g_forcePeakCycle[i] = 0.0f;
            }
        }

        readAllForces();

        for (uint8_t i = 0; i < N_TABLES; i++) {
            if (g_force[i] > g_forcePeakCycle[i]) {
                g_forcePeakCycle[i] = g_force[i];
                g_forcePeakStepCountCycle = g_stepCount;
            }

            // TODO STEP 2 CALIBRATION: re-enable guard once FORCE_GAIN/OFFSET are loaded from backend
            // Garde-fou sécurité sera validé une fois forces en Newtons (actuellement en mV bruts)
            // if (g_force[i] > FORCE_MAX_N) {
            //     handleHardReset();
            //     return;
            // }
        }

        lastInForceWindow = true;
        return;  // ★ NE PAS faire de mouvements pendant la mesure ★
    }

    // APRÈS fenêtre : appliquer corrections Dynamixel
    if (lastInForceWindow) {
        for (uint8_t i = 0; i < N_TABLES; i++) {
            if (g_dxlIds[i] == 0) continue;

            float current_mm = dxlPositionMm(i);
            auto control = getControlPhaseAndStep(i, current_mm, g_forceTarget[i], g_forcePeakCycle[i]);
            g_forcePhase[i] = control.phase;

            if (control.step_mm == 0.0f) {
                continue;
            }

            float next_mm = current_mm + control.step_mm;
            dxlGotoMm(i, next_mm);
        }
    }

    lastInForceWindow = false;
}

// ===== Protocole : sorties =================================================

static const char* modeStr() {
    switch (g_mode) {
        case Mode::IDLE:    return "IDLE";
        case Mode::HOMING:  return "HOMING";
        case Mode::READY:   return "READY";
        case Mode::RUNNING: return "RUNNING";
        default:            return "ERROR";
    }
}

static void sendAck(const String& m)  { LINK.print("ACK:");   LINK.println(m); }
static void sendDone(const String& m) { LINK.print("DONE:");  LINK.println(m); }
static void sendErr(const String& m)  { LINK.print("ERROR:"); LINK.println(m); }

static void sendStatus() {
    readAllForces();

    LINK.print("STATE:");  LINK.println(modeStr());
    LINK.print("FREQ:");   LINK.println(g_frequency, 3);

    // 4 positions séparées par des virgules (le backend parse en tableau)
    LINK.print("POSITION:");
    for (uint8_t i = 0; i < N_TABLES; i++) {
        LINK.print(dxlPositionMm(i), 2);
        if (i < N_TABLES - 1) LINK.print(',');
    }
    LINK.println();

    // 4 tensions en mV (brutes du ADC, pas converties en Newton)
    // Phase 1 : backend fait la calibration mV -> N
    // Phase 2 : firmware recevra la table de calibration et l'appliquera localement
    LINK.print("VOLT:");
    for (uint8_t i = 0; i < N_TABLES; i++) {
        LINK.print(g_force[i], 1);
        if (i < N_TABLES - 1) LINK.print(',');
    }
    LINK.println();

    LINK.print("SLAVE:");
    LINK.println(g_dxlCount == N_TABLES ? "ONLINE" : "OFFLINE");
}

// ===== Protocole : entrée ==================================================

static void handleStart() {
    digitalWrite(ENA_PIN, LOW);          // driver activé (actif bas)
    stepperSetFrequency(g_frequency);
    g_mode = Mode::RUNNING;
    sendAck("START");
}

static void handleStop() {
    stepperSetFrequency(0);              // stop pulses
    digitalWrite(ENA_PIN, HIGH);         // driver désactivé
    g_mode = Mode::IDLE;
    sendAck("STOP");
}

static void handleHome() {
    g_mode = Mode::HOMING;
    g_stepCount = 0;
    // ÉTAPE 2 : référencer les tables si nécessaire.
    g_homed = true;
    g_mode = Mode::READY;
    sendAck("HOME");
    sendDone("HOME");
}

static void handleHardReset() {
    stepperSetFrequency(0);
    digitalWrite(ENA_PIN, HIGH);
    g_mode = Mode::IDLE;
    g_homed = false;
    g_speed = 100;
    g_frequency = 0.8f;
    for (uint8_t i = 0; i < N_TABLES; i++) g_forceTarget[i] = 0;
    sendAck("HARD_RESET");
}

// SET_FORCE:<N>           -> consigne globale (les 4 cellules)
// SET_FORCE:<cell>:<N>    -> consigne d'une cellule (cell = 1..4)
static void handleSetForce(const String& arg) {
    int colon = arg.indexOf(':');
    if (colon < 0) {
        float n = arg.toFloat();
        for (uint8_t i = 0; i < N_TABLES; i++) g_forceTarget[i] = n;
    } else {
        int cell = arg.substring(0, colon).toInt();
        float n  = arg.substring(colon + 1).toFloat();
        if (cell >= 1 && cell <= N_TABLES) g_forceTarget[cell - 1] = n;
    }
    // ÉTAPE 1 : on ne fait que mémoriser. Pas de descente autonome.
    sendAck("SET_FORCE");
}

// GOTO:<table>:<mm>
static void handleGoto(const String& arg) {
    int colon = arg.indexOf(':');
    if (colon < 0) { sendErr("GOTO_FORMAT"); return; }
    int table = arg.substring(0, colon).toInt();
    float mm  = arg.substring(colon + 1).toFloat();
    if (table < 1 || table > N_TABLES) { sendErr("GOTO_TABLE"); return; }
    dxlGotoMm(table - 1, mm);
    sendAck("GOTO");
}

static void dispatch(String line) {
    line.trim();
    if (line.length() == 0) return;

    int sep = line.indexOf(':');
    String cmd = (sep >= 0) ? line.substring(0, sep) : line;
    String arg = (sep >= 0) ? line.substring(sep + 1) : "";
    cmd.trim(); cmd.toUpperCase(); arg.trim();

    if      (cmd == "HOME")       handleHome();
    else if (cmd == "START")      handleStart();
    else if (cmd == "STOP")       handleStop();
    else if (cmd == "HARD_RESET") handleHardReset();
    else if (cmd == "GET_STATUS") { sendStatus(); sendAck("GET_STATUS"); }
    else if (cmd == "SET_FREQ") {
        float f = arg.toFloat();
        if (f < 0 || f > F_ROTATION_MAX) { sendErr("FREQ_OUT_OF_RANGE"); return; }
        g_frequency = f;
        if (g_mode == Mode::RUNNING) stepperSetFrequency(f);
        sendAck("SET_FREQ");
    }
    else if (cmd == "SET_SPEED") {
        int s = arg.toInt();
        if (s < 0 || s > 10000) { sendErr("SPEED_OUT_OF_RANGE"); return; }
        g_speed = s;
        sendAck("SET_SPEED");
    }
    else if (cmd == "SET_FORCE")  handleSetForce(arg);
    else if (cmd == "GOTO")       handleGoto(arg);
    else if (cmd == "TORQUE_OFF") {                 // déverrouille (unlock) les moteurs
        for (uint8_t i = 0; i < g_dxlCount; i++) dxl.torqueOff(g_dxlIds[i]);
        sendAck("TORQUE_OFF");
    }
    else if (cmd == "TORQUE_ON") {                  // verrouille (lock) les moteurs
        for (uint8_t i = 0; i < g_dxlCount; i++) dxl.torqueOn(g_dxlIds[i]);
        sendAck("TORQUE_ON");
    }
    else if (cmd == "BLINK_MOTOR") {
        // Format: BLINK_MOTOR:<motor_id>:<duration_ms>
        int col = arg.indexOf(':');
        if (col < 0) { sendErr("BLINK_MOTOR_FORMAT"); return; }
        int motor_id = arg.substring(0, col).toInt();
        uint32_t duration_ms = arg.substring(col + 1).toInt();
        if (motor_id < 0 || motor_id >= g_dxlCount) { sendErr("BLINK_MOTOR_ID"); return; }
        if (duration_ms <= 0) duration_ms = 500;

        // Blink: toggle LED rapidly for ~duration_ms
        uint32_t start = millis();
        while (millis() - start < duration_ms) {
            dxl.ledOn(g_dxlIds[motor_id]);
            delay(100);
            dxl.ledOff(g_dxlIds[motor_id]);
            delay(100);
        }
        dxl.ledOff(g_dxlIds[motor_id]);
        sendAck("BLINK_MOTOR");
    }
    else if (cmd == "SET_RESISTANCE") {
        // Format: SET_RESISTANCE:<ohm> (both boards) or SET_RESISTANCE:<boardId>:<ohm>
        int col = arg.indexOf(':');
        int board_id = -1;
        int resistance = 30;

        if (col < 0) {
            // Format: <ohm> only
            resistance = arg.toInt();
        } else {
            // Format: <boardId>:<ohm>
            board_id = arg.substring(0, col).toInt();
            resistance = arg.substring(col + 1).toInt();
        }

        if (resistance != 30 && resistance != 90) { sendErr("RESISTANCE_VALUE"); return; }
        if (board_id >= 0 && (board_id != 0 && board_id != 1)) { sendErr("BOARD_ID"); return; }

        // D4 = Board 0 (cells 0-1), D5 = Board 1 (cells 2-3)
        // 30 Ω = relay OFF (LOW)
        // 90 Ω = relay ON  (HIGH)
        uint8_t relay_state = (resistance == 30) ? LOW : HIGH;

        if (board_id < 0) {
            // Both boards
            digitalWrite(4, relay_state);  // Pin 4 = Board 0 relay
            digitalWrite(5, relay_state);  // Pin 5 = Board 1 relay
        } else if (board_id == 0) {
            // Board 0 only (pin 4)
            digitalWrite(4, relay_state);
        } else {
            // Board 1 only (pin 5)
            digitalWrite(5, relay_state);
        }
        sendAck("SET_RESISTANCE");
    }
    else                          sendErr("UNKNOWN_COMMAND");
}

// ===== Setup / Loop ========================================================

static String g_rx;

void setup() {
    pinMode(PUL_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(ENA_PIN, OUTPUT);
    digitalWrite(DIR_PIN, LOW);          // sens horaire par défaut
    digitalWrite(ENA_PIN, HIGH);         // driver désactivé au boot

    // Initialize relays (pins 4, 5) for INA125 gain control (30 Ω vs 90 Ω)
    // Default: 30 Ω (relays OFF)
    pinMode(4, OUTPUT);                  // Pin 4 = Board 0 relay (cells 0-1)
    pinMode(5, OUTPUT);                  // Pin 5 = Board 1 relay (cells 2-3)
    digitalWrite(4, LOW);                // 30 Ω (relay 1 OFF)
    digitalWrite(5, LOW);                // 30 Ω (relay 2 OFF)

    analogReadResolution(12);            // 0..4095

    LINK.begin(LINK_BAUD);               // lien vers l'ESP8266

    dxl.begin(DXL_BAUD);
    dxl.setPortProtocolVersion(DXL_PROTOCOL);
    dxlScan();  // Scan Dynamixel IDs 1-20 on Serial1

    stepperTimerInit();

    LINK.println("ACK:BOOT");
    LINK.print("STATE:"); LINK.println(modeStr());
}

void loop() {
    // Réception des commandes ligne depuis l'ESP
    while (LINK.available()) {
        char c = (char)LINK.read();
        if (c == '\n' || c == '\r') {
            if (g_rx.length() > 0) { dispatch(g_rx); g_rx = ""; }
        } else if (g_rx.length() < 96) {
            g_rx += c;
        }
    }

    // Liaison permanente : burst de statut autonome à 10 Hz (l'ESP le cache).
    static uint32_t lastStreamMs = 0;
    uint32_t now = millis();
    if (now - lastStreamMs >= STREAM_PERIOD_MS) {
        lastStreamMs = now;
        sendStatus();
    }

    // Feedback LED : clignotement en RUNNING, fixe sinon.
    updateDxlLeds();

    // ÉTAPE 2 : Boucle fermée de force (timing critique)
    // - PENDANT fenêtre : lire forces seulement
    // - APRÈS fenêtre : appliquer corrections si erreur
    static uint32_t lastForceLoopUpdateMs = 0;
    if (now - lastForceLoopUpdateMs >= FORCE_LOOP_INTERVAL_MS) {
        lastForceLoopUpdateMs = now;
        updateForceLoop();
    }
}
