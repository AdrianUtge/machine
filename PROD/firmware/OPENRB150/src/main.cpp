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
#include "motor_init.h"

// ===== Configuration =======================================================

// --- Lien série vers l'ESP8266 ---
#define LINK            Serial3
// Phase 3: Using hardware UART1 @ 19200 on ESP8266 (fast, stable)
// Must match ESP8266 Serial1.begin(19200) in firmware/ESP8266/src/main.cpp
#define LINK_BAUD       19200      // Hardware UART1, matches ESP Serial1.begin()

// --- Streaming statut (liaison permanente) ---
// On émet le burst de statut tout seul, sans attendre de GET_STATUS.
// L'ESP cache le dernier burst -> /api/status devient instantané (pas d'A/R série).
// Increased to 400ms to prevent buffer overflow on ESP SoftwareSerial (2.5 Hz is safer than 5 Hz)
#define STREAM_PERIOD_MS 400

// --- Binary Protocol Response Codes (Phase 3) ---
#define RESP_ACK             0x00
#define RESP_DONE            0x01
#define RESP_ERROR_INVALID_CMD  0x80
#define RESP_ERROR_INVALID_ARG  0x81
#define RESP_ERROR_CRC       0x82
#define RESP_ERROR_DEVICE    0x83

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
static bool lastInForceWindow = false;

// Handshake with ESP: true after first command received
static bool esp_ready = false;

// Flag: true during binary command processing (handlers skip text responses)
static bool binary_mode_active = false;

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
float    g_forceTarget[4] = { 0, 0, 0, 0 };  // consignes (ÉTAPE 2) - Accessible to motor_init.cpp
float    g_force[4] = { 0, 0, 0, 0 };        // mesures (N) - Accessible to motor_init.cpp

static uint8_t  g_dxlIds[4] = { 0, 0, 0, 0 };
uint8_t  g_dxlCount = 0;  // Accessible to motor_init.cpp

// ===== DMA FREERUN ACQUISITION (Load Cell Integration) =======================
//
// Topologie : ADC freerun @ 87 kSPS + DMA direct → RAM
// Burst size : 8192 samples per cycle (94 ms @ 10 Hz rotation)
// Centrage : burst déclenché au TRIGGER_STEP pour avoir point bas au centre
// Recalage : correction stepCount via argmax(force peak) pour compenser drift moteur
//
// Voir PROD/docs/15_LOAD_CELL_CALIBRATION.md et TEST-PLATFORM/.../compilation_technique.md

// --- DMA Buffers (one per force cell) ---
// Large buffers to capture burst @ 87 kSPS for 94 ms (8192 samples)
// Runtime configurable via SET_FORCE_BURST (0x40) command
static int g_forceDmaBurstSize = 8192;  // Configurable at runtime (256–12000)
static volatile uint16_t g_forceBurstBuffer[4][8192] __attribute__((aligned(4)));  // Always allocate max

// Flags to track burst state per cell
static volatile bool g_forceBurstReady[4] = { false, false, false, false };
static volatile bool g_forceBurstArmed[4] = { false, false, false, false };

// DMA descriptors (must be 16-byte aligned, one per cell)
typedef struct {
  uint16_t btctrl;
  uint16_t btcnt;
  uint32_t srcaddr;
  uint32_t dstaddr;
  uint32_t descaddr;
} DmacDescriptor_t;

__attribute__((aligned(16))) static DmacDescriptor_t g_dmacDescriptors[4];
__attribute__((aligned(16))) static DmacDescriptor_t g_dmacWriteback[4];

// ADC configuration for freerun mode (@ 87 kSPS with 4x averaging)
static const float FORCE_SAMPLE_RATE = 87000.0f;  // Hz (prescaler /32, avg 4×)
static const int FORCE_OFFSET_STEPS = (int)(32000.0f * FORCE_DMA_BURST_SIZE / (2.0f * FORCE_SAMPLE_RATE));
static const int FORCE_TRIGGER_STEP = STEPS_PER_REV - FORCE_OFFSET_STEPS;  // ~1693 for 3200 steps

// === Calibration & Limites Dynamixel ========================================
// Position limites (mm) pour chaque table — établies lors du HOME
static float    g_positionMin[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
static float    g_positionMax[4] = { 96.0f, 96.0f, 96.0f, 96.0f };  // a priori

// Position cible mémorisée
static float    g_positionTarget[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

// Paramètres calibration (depuis .machine_config.ini via ESP)
static float    DXL_TORQUE_THRESHOLD = 800.0f;   // seuil de détection limite (0–1023)
static uint16_t DXL_CALIB_STEP_DELAY_MS = 50;    // délai polling lors calibration

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

            // DMA FREERUN: Trigger burst at anticipation point (centers burst on point bas)
            if (g_stepCount == FORCE_TRIGGER_STEP) {
                // Check all cells are ready (previous burst done) before starting new one
                bool allDone = true;
                for (uint8_t i = 0; i < 4; i++) {
                    if (g_forceBurstArmed[i]) {
                        allDone = false;
                        break;
                    }
                }
                if (allDone) {
                    forceStartBurstDMA();
                }
            }
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

// ===== ADC Freerun Setup ===================================================

static void forceADCSetup() {
    // Enable GCLK0 to ADC
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN |
                        GCLK_CLKCTRL_GEN_GCLK0 |
                        GCLK_CLKCTRL_ID_ADC;
    while (GCLK->STATUS.bit.SYNCBUSY);

    // Reset ADC
    ADC->CTRLA.bit.ENABLE = 0;
    while (ADC->STATUS.bit.SYNCBUSY);
    ADC->CTRLA.bit.SWRST = 1;
    while (ADC->CTRLA.bit.SWRST);

    // Reference: VDDANA/2 with gain 1/2
    ADC->REFCTRL.reg = ADC_REFCTRL_REFSEL_INTVCC1;

    // Control: prescaler /32, 12-bit resolution, freerun mode
    ADC->CTRLB.reg = ADC_CTRLB_PRESCALER_DIV32 |
                     ADC_CTRLB_RESSEL_12BIT |
                     ADC_CTRLB_FREERUN;
    while (ADC->STATUS.bit.SYNCBUSY);

    // Input: single-ended, pin A0 (will be changed per cell), gain 1/2
    ADC->INPUTCTRL.reg = ADC_INPUTCTRL_MUXNEG_GND |
                         ADC_INPUTCTRL_MUXPOS_PIN0 |
                         ADC_INPUTCTRL_GAIN_DIV2;
    while (ADC->STATUS.bit.SYNCBUSY);

    // Averaging: 4 samples, adjustment right-shift 2 → ~87 kSPS effective
    ADC->AVGCTRL.reg = ADC_AVGCTRL_SAMPLENUM_4 |
                       ADC_AVGCTRL_ADJRES(2);
    while (ADC->STATUS.bit.SYNCBUSY);

    // Sampling: 4 clock cycles (SAMD21 requires >= 2)
    ADC->SAMPCTRL.reg = ADC_SAMPCTRL_SAMPLEN(4);
    while (ADC->STATUS.bit.SYNCBUSY);
}

// ===== DMA Setup ===========================================================

static void forceDMASetup() {
    // Enable DMA clocks
    PM->AHBMASK.bit.DMAC_ = 1;
    PM->APBBMASK.bit.DMAC_ = 1;

    // Reset DMAC
    DMAC->CTRL.bit.DMAENABLE = 0;
    DMAC->CTRL.bit.SWRST = 1;

    // Configure DMA
    DMAC->BASEADDR.reg = (uint32_t)g_dmacDescriptors;
    DMAC->WRBADDR.reg = (uint32_t)g_dmacWriteback;
    DMAC->CTRL.reg = DMAC_CTRL_DMAENABLE | DMAC_CTRL_LVLEN(0xF);

    // Configure 4 DMA channels (one per force cell A1-A4 = ADC AIN1-4)
    for (uint8_t ch = 0; ch < 4; ch++) {
        DMAC->CHID.reg = ch;
        DMAC->CHCTRLB.reg = DMAC_CHCTRLB_TRIGSRC(ADC_DMAC_ID_RESRDY) |
                            DMAC_CHCTRLB_TRIGACT_BEAT |
                            DMAC_CHCTRLB_LVL(0);
        DMAC->CHINTENSET.reg = DMAC_CHINTENSET_TCMPL;
    }

    // Enable DMA interrupts
    NVIC_EnableIRQ(DMAC_IRQn);
    NVIC_SetPriority(DMAC_IRQn, 1);  // Lower priority than TC3 (stepper)
}

// Arm DMA descriptor for one cell
static void forceArmDMABurst(uint8_t cell) {
    DMAC->CHID.reg = cell;
    DMAC->CHCTRLA.bit.ENABLE = 0;

    int burstSize = g_forceDmaBurstSize;  // Use runtime-configurable size

    g_dmacDescriptors[cell].btctrl = DMAC_BTCTRL_VALID |
                                     DMAC_BTCTRL_BEATSIZE_HWORD |
                                     DMAC_BTCTRL_DSTINC |
                                     DMAC_BTCTRL_BLOCKACT_INT;
    g_dmacDescriptors[cell].btcnt = burstSize;
    g_dmacDescriptors[cell].srcaddr = (uint32_t)&ADC->RESULT.reg;
    g_dmacDescriptors[cell].dstaddr = (uint32_t)g_forceBurstBuffer[cell] +
                                      burstSize * sizeof(uint16_t);
    g_dmacDescriptors[cell].descaddr = 0;

    DMAC->CHID.reg = cell;
    DMAC->CHCTRLA.bit.ENABLE = 1;
}

// Start a DMA burst for all 4 cells
static void forceStartBurstDMA() {
    for (uint8_t cell = 0; cell < 4; cell++) {
        g_forceBurstReady[cell] = false;
        g_forceBurstArmed[cell] = true;
        forceArmDMABurst(cell);
    }

    // Start ADC freerun
    ADC->CTRLA.bit.ENABLE = 1;
    while (ADC->STATUS.bit.SYNCBUSY);
    ADC->SWTRIG.bit.START = 1;
    while (ADC->STATUS.bit.SYNCBUSY);
}

// ===== DMA ISR =============================================================

extern "C" void DMAC_Handler() {
    // Multiple channels fire this handler
    for (uint8_t ch = 0; ch < 4; ch++) {
        DMAC->CHID.reg = ch;
        if (DMAC->CHINTFLAG.bit.TCMPL) {
            DMAC->CHINTFLAG.bit.TCMPL = 1;
            g_forceBurstReady[ch] = true;
            g_forceBurstArmed[ch] = false;
        }
    }

    // Disable ADC after all 4 bursts complete
    ADC->CTRLA.bit.ENABLE = 0;
    while (ADC->STATUS.bit.SYNCBUSY);
}

// ===== Force : lecture INA125 (DMA burst, centré sur point bas) =============

// Read force from completed DMA burst buffer, with auto-recalibration on force peak
static float readForcemVFromBurst(uint8_t cell) {
    if (!g_forceBurstReady[cell]) {
        return 0.0f;  // Burst not ready yet
    }

    g_forceBurstReady[cell] = false;

    int burstSize = g_forceDmaBurstSize;  // Use runtime-configurable size

    // Find peak (max ADC sample in burst)
    int iPeakIdx = 0;
    uint16_t peakVal = g_forceBurstBuffer[cell][0];
    for (int i = 1; i < burstSize; i++) {
        if (g_forceBurstBuffer[cell][i] > peakVal) {
            peakVal = g_forceBurstBuffer[cell][i];
            iPeakIdx = i;
        }
    }

    // Auto-recalibration: correct stepCount drift based on peak position
    // Peak should be at burstSize/2; if offset, we've lost steps
    int deltaIdx = iPeakIdx - (burstSize / 2);
    if (abs(deltaIdx) > 10) {  // Only correct if drift > 10 samples (≈3.7 steps)
        float stepDrift = deltaIdx * (32000.0f / FORCE_SAMPLE_RATE);
        g_stepCount += (int)stepDrift;
        if (g_stepCount < 0) g_stepCount = 0;
        if (g_stepCount >= STEPS_PER_REV) g_stepCount -= STEPS_PER_REV;

        // Log significant corrections
        if (abs(deltaIdx) > 50) {
            Serial.print("[force] Peak drift correction: ");
            Serial.print(deltaIdx); Serial.print(" samples, ");
            Serial.print((int)stepDrift); Serial.println(" steps");
        }
    }

    // Average all burst samples → counts
    uint32_t sumCounts = 0;
    for (int i = 0; i < burstSize; i++) {
        sumCounts += g_forceBurstBuffer[cell][i];
    }
    float avgCounts = (float)sumCounts / burstSize;

    // Convert counts → mV
    return (avgCounts / ADC_RESOLUTION_STEPS) * ADC_VREF;
}

// Fallback: simple read when DMA burst not available
static float readForcemVSimple(uint8_t cell) {
    uint32_t acc = 0;
    for (uint8_t i = 0; i < FORCE_BURST; i++) acc += analogRead(FORCE_PINS[cell]);
    float counts = (float)acc / FORCE_BURST;
    return (counts / ADC_RESOLUTION_STEPS) * ADC_VREF;
}

// Phase 2 : will be used for local force-feedback loop (after backend calibration)
// static float readForceN(uint8_t cell) {
//     float mv = readForcemVFromBurst(cell);
//     if (mv == 0.0f) mv = readForcemVSimple(cell);
//     return (mv / ADC_VREF) * ADC_RESOLUTION_STEPS * FORCE_GAIN[cell] + FORCE_OFFSET[cell];
// }

void readAllForces() {
    // Use DMA burst (87 kSPS with auto-recalibration) when available, fallback to simple read
    for (uint8_t i = 0; i < N_TABLES; i++) {
        float mV = readForcemVFromBurst(i);
        if (mV == 0.0f) {
            mV = readForcemVSimple(i);  // Fallback if burst not ready
        }
        g_force[i] = mV;
    }
}

// ===== Dynamixel ===========================================================

static void dxlScan() {
    g_dxlCount = 0;
    Serial.print("[dxlScan] Starting scan (IDs 1-"); Serial.print(DXL_SCAN_MAX);
    Serial.print(", max "); Serial.print(N_TABLES); Serial.println(" motors)");

    for (uint8_t id = 1; id <= DXL_SCAN_MAX && g_dxlCount < N_TABLES; id++) {
        Serial.print("[dxlScan] Pinging ID "); Serial.print(id); Serial.print("... ");
        if (dxl.ping(id)) {
            Serial.println("✓ FOUND");
            g_dxlIds[g_dxlCount++] = id;
        } else {
            Serial.println("✗ no response");
        }
    }

    Serial.print("[dxlScan] Total found: "); Serial.println(g_dxlCount);
    for (uint8_t i = 0; i < g_dxlCount; i++) {
        Serial.print("[dxlScan] Motor "); Serial.print(i); Serial.print(" = Dynamixel ID ");
        Serial.println(g_dxlIds[i]);
    }

    Serial.println("[dxlScan] Initializing motors (torque off, mode=position, torque on)...");
    for (uint8_t i = 0; i < g_dxlCount; i++) {
        dxl.torqueOff(g_dxlIds[i]);
        dxl.setOperatingMode(g_dxlIds[i], OP_POSITION);
        dxl.torqueOn(g_dxlIds[i]);
        Serial.print("[dxlScan] Motor "); Serial.print(i); Serial.println(" initialized");
    }
    Serial.println("[dxlScan] Done!");
}

float dxlPositionMm(uint8_t table) {
    if (table >= g_dxlCount) return 0.0f;
    float pos = dxl.getPresentPosition(g_dxlIds[table]);  // unités Dynamixel
    return pos / DXL_PER_MM;
}

void dxlGotoMm(uint8_t table, float mm) {
    if (table >= g_dxlCount) return;
    dxl.setGoalPosition(g_dxlIds[table], mm * DXL_PER_MM);
    g_positionTarget[table] = mm;  // Mémoriser cible
}

// Lire le courant du moteur (détection obstacle/fin de course)
static float dxlGetCurrent(uint8_t table) {
    if (table >= g_dxlCount) return 0.0f;
    return (float)dxl.getPresentCurrent(g_dxlIds[table]);
}

// Calibration d'une table : remontée progressive, détection fin de course par torque
// Retourne true si succès, false si timeout/erreur
static bool calibrateTable(uint8_t table, uint16_t timeout_ms = 30000) {
    if (table >= g_dxlCount) {
        Serial.print("[calibrate] Table "); Serial.print(table);
        Serial.println(" : moteur absent (dxlCount)");
        return false;
    }

    const float MAX_GOTO_MM = 100.0f;  // position max a priori
    const float CALIB_RETREAT_MM = 2.0f;  // recul après détection limite
    uint32_t start_ms = millis();

    Serial.print("[calibrate] Table "); Serial.print(table); Serial.println(" : démarrage remontée");

    // Remontée progressive vers MAX
    dxlGotoMm(table, MAX_GOTO_MM);

    // Polling du courant jusqu'à limite ou timeout
    float calib_position = 0.0f;

    while (millis() - start_ms < timeout_ms) {
        float current_pos = dxlPositionMm(table);
        float current_load = dxlGetCurrent(table);

        // Serial.print("[calibrate] T"); Serial.print(table);
        // Serial.print(" pos="); Serial.print(current_pos);
        // Serial.print(" load="); Serial.println(current_load);

        if (current_load > DXL_TORQUE_THRESHOLD) {
            // Fin de course détectée
            calib_position = current_pos;
            Serial.print("[calibrate] Table "); Serial.print(table);
            Serial.print(" : FIN DE COURSE détectée à "); Serial.print(calib_position);
            Serial.print(" mm (load="); Serial.print(current_load); Serial.println(")");
            break;
        }

        delay(DXL_CALIB_STEP_DELAY_MS);
    }

    // Vérifier si fin de course trouvée
    if (calib_position < 1.0f) {
        Serial.print("[calibrate] Table "); Serial.print(table);
        Serial.println(" : TIMEOUT - aucune fin de course détectée");
        return false;
    }

    // Recul de 2 mm (dégagement)
    float retreat_pos = max(0.0f, calib_position - CALIB_RETREAT_MM);
    Serial.print("[calibrate] Table "); Serial.print(table);
    Serial.print(" : recul à "); Serial.print(retreat_pos); Serial.println(" mm");
    dxlGotoMm(table, retreat_pos);

    delay(500);  // attendre stabilisation

    // Sauvegarder limites
    g_positionMin[table] = 0.0f;
    g_positionMax[table] = calib_position - CALIB_RETREAT_MM;

    Serial.print("[calibrate] Table "); Serial.print(table);
    Serial.print(" : SUCCÈS limites=["); Serial.print(g_positionMin[table]);
    Serial.print(", "); Serial.print(g_positionMax[table]); Serial.println("]");

    return true;
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
    // Skip force control loop while init is running
    if (initIsRunning()) return;
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

// CRC8 checksum (matches ESP/backend firmware)
static uint8_t crc8_calc(const uint8_t* data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = ((crc << 1) ^ 0x07) & 0xFF;
            } else {
                crc = (crc << 1) & 0xFF;
            }
        }
    }
    return crc;
}

// Send binary response frame: [0x52] [RESULT_CODE] [CRC8]
static void sendResponseBinary(uint8_t result_code) {
    uint8_t frame[3] = { 0x52, result_code, 0x00 };
    frame[2] = crc8_calc(frame, 2);
    LINK.write(frame, 3);
}

// Legacy text responses (kept for debugging, but not used in Phase 3)
static void sendAck(const String& m)  { LINK.print("ACK:");   LINK.println(m); }
static void sendDone(const String& m) { LINK.print("DONE:");  LINK.println(m); }
static void sendErr(const String& m)  { LINK.print("ERROR:"); LINK.println(m); }
static void sendCalib(uint8_t table, float min_mm, float max_mm) {
    LINK.print("CALIB:"); LINK.print(table);
    LINK.print(":"); LINK.print(min_mm, 1);
    LINK.print(":"); LINK.println(max_mm, 1);
}

static void sendStatusBinary() {
    // Binary STATUS frame: [0xS] [FREQ:2 LE] [POS[4]:8] [FORCE[4]:8] [CRC8]
    // Total: 20 bytes
    readAllForces();

    uint8_t frame[20] = { 0x53 };  // Type = 'S'

    // FREQ (Hz × 10, u16 LE)
    uint16_t freq_hz10 = (uint16_t)(g_frequency * 10.0f);
    frame[1] = (uint8_t)(freq_hz10 & 0xFF);
    frame[2] = (uint8_t)((freq_hz10 >> 8) & 0xFF);

    // POSITIONS (mm × 10, u16 LE, 4×2 bytes)
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t pos_mm10 = (uint16_t)(dxlPositionMm(i) * 10.0f);
        frame[3 + i*2] = (uint8_t)(pos_mm10 & 0xFF);
        frame[4 + i*2] = (uint8_t)((pos_mm10 >> 8) & 0xFF);
    }

    // FORCES (mV, u16 LE, 4×2 bytes)
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t force_mv = (uint16_t)g_force[i];
        frame[11 + i*2] = (uint8_t)(force_mv & 0xFF);
        frame[12 + i*2] = (uint8_t)((force_mv >> 8) & 0xFF);
    }

    // CRC8
    frame[19] = crc8_calc(frame, 19);

    // Send all 20 bytes
    LINK.write(frame, 20);
}

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
    if (!binary_mode_active) sendAck("START");
}

static void handleStop() {
    stepperSetFrequency(0);              // stop pulses
    digitalWrite(ENA_PIN, HIGH);         // driver désactivé
    g_mode = Mode::IDLE;
    if (!binary_mode_active) sendAck("STOP");
}

static void handleHome() {
    g_mode = Mode::HOMING;
    g_stepCount = 0;

    if (!binary_mode_active) sendAck("HOME");

    // Auto-calibration des tables : remontée + détection fin de course par torque
    Serial.println("[handleHome] Calibration des 4 tables...");
    bool all_success = true;
    for (uint8_t i = 0; i < g_dxlCount; i++) {
        if (!calibrateTable(i)) {
            all_success = false;
            if (!binary_mode_active) sendErr("HOME_CALIB_FAIL");
        } else {
            // Envoyer le statut de calibration
            if (!binary_mode_active) sendCalib(i + 1, g_positionMin[i], g_positionMax[i]);
        }
    }

    g_homed = all_success;
    g_mode = all_success ? Mode::READY : Mode::ERROR;

    if (!binary_mode_active) sendDone("HOME");
}

static void handleHardReset() {
    stepperSetFrequency(0);
    digitalWrite(ENA_PIN, HIGH);
    g_mode = Mode::IDLE;
    g_homed = false;
    g_speed = 100;
    g_frequency = 0.8f;
    for (uint8_t i = 0; i < N_TABLES; i++) g_forceTarget[i] = 0;
    initStop();  // Stop init if running
    if (!binary_mode_active) sendAck("HARD_RESET");
}

static void handleInitStart(const String& arg) {
    // Format: INIT_START:<target_pos_mm>:<descent_rate_mm_per_min>
    int colon1 = arg.indexOf(':');
    if (colon1 < 0) {
        if (!binary_mode_active) sendErr("INIT_START_FORMAT");
        return;
    }
    float target_mm = arg.substring(0, colon1).toFloat();
    float descent_rate = arg.substring(colon1 + 1).toFloat();

    initStart(target_mm, descent_rate);
    if (!binary_mode_active) sendAck("INIT_START");
}

static void handleInitStop() {
    initStop();
    if (!binary_mode_active) sendAck("INIT_STOP");
}

static void handleInitStatus() {
    InitState state = initGetState();
    const char* phase_str;
    switch (state.phase) {
        case INIT_IDLE:              phase_str = "IDLE"; break;
        case INIT_PHASE1_DESCENT:    phase_str = "PHASE1"; break;
        case INIT_PHASE2_FINE_DESCENT: phase_str = "PHASE2"; break;
        case INIT_PHASE3_FORCE_HOLD: phase_str = "PHASE3"; break;
        case INIT_COMPLETE:          phase_str = "COMPLETE"; break;
        case INIT_ERROR:             phase_str = "ERROR"; break;
        default:                     phase_str = "UNKNOWN"; break;
    }

    LINK.print("INIT_STATUS:");
    LINK.print(phase_str);
    LINK.print(',');
    LINK.print(state.progress_percent);
    LINK.print(',');
    LINK.print(state.elapsed_ms);
    LINK.print(',');
    LINK.print(state.force_peaks[0], 1);
    LINK.print(',');
    LINK.print(state.force_peaks[1], 1);
    LINK.print(',');
    LINK.print(state.force_peaks[2], 1);
    LINK.print(',');
    LINK.print(state.force_peaks[3], 1);
    LINK.print(',');
    // complete_mask: bitmask of completed motors
    uint8_t mask = 0;
    for (uint8_t i = 0; i < N_TABLES; i++) {
        if (state.complete[i]) mask |= (1 << i);
    }
    LINK.println(mask, HEX);
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
    if (!binary_mode_active) sendAck("SET_FORCE");
}

// GOTO:<table>:<mm>
static void handleBlink(uint8_t motor_id, uint32_t duration_ms) {
    if (motor_id >= g_dxlCount) return;
    if (duration_ms <= 0) duration_ms = 500;

    uint32_t start = millis();
    while (millis() - start < duration_ms) {
        dxl.ledOn(g_dxlIds[motor_id]);
        delay(100);
        dxl.ledOff(g_dxlIds[motor_id]);
        delay(100);
    }
    dxl.ledOff(g_dxlIds[motor_id]);
}

static void handleGoto(const String& arg) {
    int colon = arg.indexOf(':');
    if (colon < 0) {
        if (!binary_mode_active) sendErr("GOTO_FORMAT");
        return;
    }
    int table = arg.substring(0, colon).toInt();
    float mm  = arg.substring(colon + 1).toFloat();

    // Validation: table number
    if (table < 1 || table > N_TABLES) {
        if (!binary_mode_active) sendErr("GOTO_TABLE");
        return;
    }

    // Validation: machine state (READY ou IDLE uniquement)
    if (g_mode != Mode::READY && g_mode != Mode::IDLE) {
        if (!binary_mode_active) sendErr("GOTO_STATE");
        return;
    }

    uint8_t table_idx = table - 1;

    // Validation: limites de position calibrées
    if (mm < g_positionMin[table_idx] || mm > g_positionMax[table_idx]) {
        if (!binary_mode_active) {
            sendErr("GOTO_LIMIT");
            Serial.print("[GOTO] Table "); Serial.print(table);
            Serial.print(" position "); Serial.print(mm);
            Serial.print(" hors limites ["); Serial.print(g_positionMin[table_idx]);
            Serial.print(", "); Serial.print(g_positionMax[table_idx]); Serial.println("]");
        }
        return;
    }

    // Validation: moteur présent
    if (table_idx >= g_dxlCount) {
        if (!binary_mode_active) sendErr("SLAVE_OFFLINE");
        return;
    }

    // Exécution
    dxlGotoMm(table_idx, mm);
    if (!binary_mode_active) sendAck("GOTO");
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
    else if (cmd == "INIT_START") handleInitStart(arg);
    else if (cmd == "INIT_STOP")  handleInitStop();
    else if (cmd == "INIT_STATUS") handleInitStatus();
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
        Serial.print("[DEBUG] BLINK_MOTOR arg='"); Serial.print(arg); Serial.println("'");
        int col = arg.indexOf(':');
        if (col < 0) { sendErr("BLINK_MOTOR_FORMAT"); return; }
        int motor_id = arg.substring(0, col).toInt();
        uint32_t duration_ms = arg.substring(col + 1).toInt();
        Serial.print("[DEBUG] motor_id="); Serial.print(motor_id);
        Serial.print(" duration_ms="); Serial.print(duration_ms);
        Serial.print(" g_dxlCount="); Serial.println(g_dxlCount);
        if (motor_id < 0 || motor_id >= g_dxlCount) {
            Serial.print("[ERROR] motor_id out of range: "); Serial.print(motor_id);
            Serial.print(" >= "); Serial.println(g_dxlCount);
            sendErr("BLINK_MOTOR_ID");
            return;
        }
        if (duration_ms <= 0) duration_ms = 500;
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
    else if (cmd == "SCAN_DXL") {
        // Debug: force re-scan and report results
        Serial.println("[SCAN_DXL] Scanning...");
        dxlScan();
        // Send results as comma-separated list
        LINK.print("DXL_SCAN:");
        LINK.print(g_dxlCount);
        for (uint8_t i = 0; i < g_dxlCount; i++) {
            LINK.print(",");
            LINK.print(g_dxlIds[i]);
        }
        LINK.println();
        sendAck("SCAN_DXL");
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

// ===== Binary Command Handler =====
static void handleBinaryCommand(const uint8_t* frame, size_t len) {
    if (len < 2) {
        sendResponseBinary(RESP_ERROR_INVALID_CMD);
        return;
    }

    // Debug: log command received
    Serial.print("[Binary] Cmd=0x");
    Serial.print(frame[1], HEX);
    Serial.print(" len=");
    Serial.println(len);

    binary_mode_active = true;  // Signal to handlers: send binary response, not text
    uint8_t cmd_id = frame[1];
    bool handled = false;

    switch (cmd_id) {
        case 0x01:  // START
            handleStart();
            sendResponseBinary(RESP_ACK);
            handled = true;
            break;
        case 0x02:  // STOP
            handleStop();
            sendResponseBinary(RESP_ACK);
            handled = true;
            break;
        case 0x03:  // HARD_RESET
            handleHardReset();
            sendResponseBinary(RESP_ACK);
            handled = true;
            break;
        case 0x10:  // SET_FREQ
            if (len >= 3) {
                float f = (float)frame[2];
                if (f < 0 || f > F_ROTATION_MAX) {
                    sendResponseBinary(RESP_ERROR_INVALID_ARG);
                } else {
                    g_frequency = f;
                    if (g_mode == Mode::RUNNING) stepperSetFrequency(f);
                    sendResponseBinary(RESP_ACK);
                    handled = true;
                }
            }
            break;
        case 0x20:  // GOTO (binary: optimized)
            if (len >= 5) {
                uint8_t table = frame[2];
                uint16_t pos_mm10 = frame[3] | (frame[4] << 8);
                float pos_mm = pos_mm10 / 10.0f;

                // Validation: table number
                if (table < 1 || table > N_TABLES) {
                    sendResponseBinary(RESP_ERROR_INVALID_ARG);
                    break;
                }

                // Validation: machine state
                if (g_mode != Mode::READY && g_mode != Mode::IDLE) {
                    sendResponseBinary(RESP_ERROR_INVALID_ARG);
                    break;
                }

                uint8_t table_idx = table - 1;

                // Validation: limites & slave présent
                if (table_idx >= g_dxlCount ||
                    pos_mm < g_positionMin[table_idx] ||
                    pos_mm > g_positionMax[table_idx]) {
                    sendResponseBinary(RESP_ERROR_INVALID_ARG);
                } else {
                    // Direct call (no String reconstruction)
                    dxlGotoMm(table_idx, pos_mm);
                    sendResponseBinary(RESP_ACK);
                    handled = true;
                }
            }
            break;
        case 0x30:  // MOTOR_BLINK
            if (len >= 5) {
                uint8_t motor_id = frame[2];
                uint16_t duration_ms = frame[3] | (frame[4] << 8);
                handleBlink(motor_id, duration_ms);
                sendResponseBinary(RESP_ACK);
                handled = true;
            }
            break;
        case 0x31:  // SCAN_DXL
            dxlScan();
            sendResponseBinary(RESP_ACK);
            handled = true;
            break;
        case 0x40:  // SET_FORCE_BURST (dynamic sample count)
            if (len >= 4) {
                // Unpack size as little-endian u16
                uint16_t new_burst_size = frame[2] | (frame[3] << 8);

                // Validate range (256–12000)
                if (new_burst_size < 256 || new_burst_size > 12000) {
                    sendResponseBinary(RESP_ERROR_INVALID_ARG);
                } else {
                    // Recalculate trigger step for new burst size
                    int old_burst = g_forceDmaBurstSize;
                    int old_trigger = FORCE_TRIGGER_STEP;

                    g_forceDmaBurstSize = new_burst_size;
                    // FORCE_TRIGGER_STEP = STEPS_PER_REV - (int)(32000 * size / (2 * 87000))
                    int new_trigger = (int)(3200 - (32000 * new_burst_size / (2.0f * 87000)));

                    // Adjust step counter if timing shifted
                    int trigger_delta = new_trigger - old_trigger;
                    g_stepCount += trigger_delta;
                    if (g_stepCount < 0) g_stepCount = 0;
                    if (g_stepCount >= STEPS_PER_REV) g_stepCount -= STEPS_PER_REV;

                    Serial.print("[force] Burst size updated: ");
                    Serial.print(old_burst); Serial.print(" → ");
                    Serial.println(new_burst_size);

                    sendResponseBinary(RESP_ACK);
                    handled = true;
                }
            }
            break;
        case 0xF0:  // GET_STATUS
            sendStatusBinary();  // Send binary STATUS frame
            sendResponseBinary(RESP_ACK);
            handled = true;
            break;
        default:
            // Unknown command
            sendResponseBinary(RESP_ERROR_INVALID_CMD);
            break;
    }
    binary_mode_active = false;  // Reset flag after command processing
}

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

    // Initialize DMA Freerun acquisition for load cells
    forceADCSetup();
    forceDMASetup();

    LINK.begin(LINK_BAUD);               // lien vers l'ESP8266

    dxl.begin(DXL_BAUD);
    dxl.setPortProtocolVersion(DXL_PROTOCOL);
    dxlScan();  // Scan Dynamixel IDs 1-20 on Serial1

    stepperTimerInit();

    // Don't send boot messages on LINK (SoftwareSerial) to avoid flooding ESP at startup
    // Status streaming begins after 3-second delay in loop()
}

void loop() {
    // Réception des commandes depuis l'ESP (binaire ou texte)
    static uint8_t bin_rx_buffer[64];
    static size_t bin_rx_pos = 0;
    static uint32_t bin_rx_last_byte_ms = 0;
    static uint32_t last_debug_time = 0;

    // Debug: log if Serial3 is receiving data
    uint32_t now = millis();
    if (now - last_debug_time > 5000) {
        last_debug_time = now;
        int avail = LINK.available();
        if (avail > 0) Serial.print("[Serial3] ");
        Serial.print("Bytes available: ");
        Serial.println(avail);
    }

    while (LINK.available()) {
        uint8_t byte = LINK.read();
        bin_rx_last_byte_ms = millis();

        // Skip debug logging in hot path (too slow)

        // Check if it's a binary frame (starts with 0x43 = 'C' for command)
        if (byte == 0x43 || (bin_rx_pos > 0 && bin_rx_pos < sizeof(bin_rx_buffer))) {
            bin_rx_buffer[bin_rx_pos++] = byte;

            // Try to detect complete frame by validating CRC
            if (bin_rx_pos >= 3 && bin_rx_pos <= 8) {
                // Binary frame format: [TYPE:1][CMD:1][ARGS:0-5][CRC8:1]
                // Validate CRC8 of current position as potential frame end
                uint8_t calc_crc = 0xFF;
                for (size_t i = 0; i < bin_rx_pos - 1; i++) {
                    calc_crc ^= bin_rx_buffer[i];
                    for (int j = 0; j < 8; j++) {
                        if (calc_crc & 0x80) {
                            calc_crc = ((calc_crc << 1) ^ 0x07) & 0xFF;
                        } else {
                            calc_crc = (calc_crc << 1) & 0xFF;
                        }
                    }
                }

                if (calc_crc == bin_rx_buffer[bin_rx_pos - 1]) {
                    // Valid CRC! Frame is complete
                    // Dispatch binary command (no logging to avoid CPU lock)
                    handleBinaryCommand(bin_rx_buffer, bin_rx_pos);

                    // Mark ESP as ready (handshake: ESP sent first command)
                    if (!esp_ready) {
                        esp_ready = true;
                        // Only log once at handshake, not every frame
                        Serial.println("[Serial3] ✓ ESP ready! Starting telemetry stream");
                    }
                    bin_rx_pos = 0;
                }
            }

            // Safety: if buffer gets too large, reset
            if (bin_rx_pos >= sizeof(bin_rx_buffer)) {
                bin_rx_pos = 0;  // Silently discard (avoid CPU lock)
            }
        } else {
            // Text mode: wait for newline
            char c = (char)byte;
            if (c == '\n' || c == '\r') {
                if (g_rx.length() > 0) { dispatch(g_rx); g_rx = ""; }
            } else if (g_rx.length() < 96) {
                g_rx += c;
            }
        }
    }

    // Timeout: if no bytes for 100ms, discard incomplete binary frame
    if (bin_rx_pos > 0 && (millis() - bin_rx_last_byte_ms > 100)) {
        bin_rx_pos = 0;  // Silently discard (avoid CPU lock)
    }

    // Liaison permanente : burst de statut autonome à 2.5 Hz (STREAM_PERIOD_MS = 400ms, l'ESP le cache).
    // Start streaming binary STATUS frames only after ESP has booted (3s delay) and sent at least one command (handshake)
    // Increased STREAM_PERIOD_MS to 400ms to prevent SoftwareSerial buffer overflow on ESP
    static uint32_t lastStreamMs = 0;
    if (esp_ready && now >= 3000 && (now - lastStreamMs >= STREAM_PERIOD_MS)) {
        lastStreamMs = now;
        sendStatusBinary();  // Use binary protocol (Phase 3)
    }

    // Feedback LED : clignotement en RUNNING, fixe sinon.
    updateDxlLeds();

    // ÉTAPE 2 : Boucle fermée de force (timing critique)
    // - PENDANT fenêtre : lire forces seulement
    // - APRÈS fenêtre : appliquer corrections si erreur
    static uint32_t lastForceLoopUpdateMs = 0;
    if (now - lastForceLoopUpdateMs >= FORCE_LOOP_INTERVAL_MS) {
        lastForceLoopUpdateMs = now;
        initUpdate();      // Update init state machine (50ms interval)
        updateForceLoop(); // Update force control loop (skipped during init)
    }
}
