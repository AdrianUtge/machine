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
#define LINK_BAUD       19200      // SoftwareSerial côté ESP -> baud modéré, fiable

// --- Stepper DM542T ---
static const uint8_t PUL_PIN = 6;
static const uint8_t DIR_PIN = 7;
static const uint8_t ENA_PIN = 8;
static const int     STEPS_PER_REV = 3200;   // config DIP DM542T (µstep 1/16)
static const float   F_ROTATION_MAX = 10.0f; // Hz (spec banc)

// --- Cellules de force (INA125 -> ADC) ---
static const uint8_t FORCE_PINS[4] = { A1, A2, A3, A4 };
static const uint8_t FORCE_BURST   = 10;     // échantillons moyennés / mesure (anti-bruit)
// Calibration ADC -> Newton (PLACEHOLDER — à régler avec ton capteur 50 N) :
static float  FORCE_GAIN[4]   = { 1.0f, 1.0f, 1.0f, 1.0f };  // N par count ADC
static float  FORCE_OFFSET[4] = { 0.0f, 0.0f, 0.0f, 0.0f };  // count ADC à vide (tare)
static const float FORCE_MAX_N = 49.0f;      // garde-fou capteur (cellule 50 N) — ÉTAPE 2

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

static float readForceN(uint8_t cell) {
    uint32_t acc = 0;
    for (uint8_t i = 0; i < FORCE_BURST; i++) acc += analogRead(FORCE_PINS[cell]);
    float counts = (float)acc / FORCE_BURST;
    return (counts - FORCE_OFFSET[cell]) * FORCE_GAIN[cell];
}

static void readAllForces() {
    for (uint8_t i = 0; i < N_TABLES; i++) g_force[i] = readForceN(i);
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

    // 4 forces séparées par des virgules
    LINK.print("FORCE:");
    for (uint8_t i = 0; i < N_TABLES; i++) {
        LINK.print(g_force[i], 3);
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

    analogReadResolution(12);            // 0..4095

    LINK.begin(LINK_BAUD);               // lien vers l'ESP8266

    dxl.begin(DXL_BAUD);
    dxl.setPortProtocolVersion(DXL_PROTOCOL);
    dxlScan();

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

    // ÉTAPE 2 (à venir) : si Mode::RUNNING, au point bas faire un burst de 10
    // mesures par cellule, comparer aux consignes g_forceTarget[], descendre
    // 1 micro-pas / cycle, remonter vite si dépassement, garde-fou 49 N.
}
