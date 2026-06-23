/**
 * motor_init.cpp — Implementation of 3-phase motor initialization/force calibration
 *
 * Phase 1: Fast descent to predetermined height (e.g., 50mm)
 * Phase 2: Very slow descent (10mm/3min) until force target reached per motor
 * Phase 3: Verify force stability within deadband (±10mV)
 *
 * Blocking process: max 120 seconds total
 */

#include "motor_init.h"

// Forward declarations from main.cpp
extern uint8_t g_dxlCount;
extern uint8_t g_dxlIds[4];
extern float g_forceTarget[4];
extern float g_force[4];

extern float dxlPositionMm(uint8_t table);
extern void dxlGotoMm(uint8_t table, float mm);
extern void readAllForces();

// Init state
InitState g_initState = {};
bool g_initRunning = false;

// Constants
static const uint32_t INIT_MAX_DURATION_MS = 120000;  // 2 minutes
static const float INIT_PHASE1_STEP_MM = 1.0f;        // 1mm per cycle in Phase 1
static const uint32_t INIT_PHASE1_INTERVAL_MS = 50;   // 50ms per step
static const float INIT_PHASE2_RATE_MM_PER_MIN = 3.33f; // 10mm/3min default (0.0556 mm/50ms)
static const float FORCE_DEADBAND = 0.01f;            // ±10mV tolerance

void initStart(float target_position_mm, float descent_rate_mm_per_min) {
  // Sanity checks
  if (g_dxlCount == 0) {
    g_initState.error_code = 0x03;  // Motor error
    g_initState.phase = INIT_ERROR;
    g_initRunning = false;
    return;
  }

  // Initialize state
  g_initState.phase = INIT_PHASE1_DESCENT;
  g_initState.start_time_ms = millis();
  g_initState.target_position_mm = target_position_mm;
  g_initState.descent_rate_mm_per_min = descent_rate_mm_per_min;
  g_initState.active_motors = 0xFF;  // All motors active (bitmask)
  g_initState.elapsed_ms = 0;
  g_initState.error_code = 0;
  g_initState.progress_percent = 0;

  for (uint8_t i = 0; i < 4; i++) {
    g_initState.force_peaks[i] = 0.0f;
    g_initState.complete[i] = false;
  }

  g_initRunning = true;
}

void initStop() {
  g_initRunning = false;
  g_initState.phase = INIT_IDLE;
  g_initState.active_motors = 0;
  g_initState.error_code = 0;
}

bool initIsRunning() {
  return g_initRunning;
}

InitState initGetState() {
  return g_initState;
}

// Phase 1: Fast descent to target height
static void updatePhase1() {
  static uint32_t lastPhase1Update = 0;
  const uint32_t PHASE1_UPDATE_INTERVAL_MS = 500;  // Update position every 500ms (not every 50ms)

  uint32_t now = millis();
  if (now - lastPhase1Update < PHASE1_UPDATE_INTERVAL_MS) {
    return;  // Too soon, skip this update
  }
  lastPhase1Update = now;

  bool all_at_target = true;

  for (uint8_t i = 0; i < g_dxlCount; i++) {
    float current_mm = dxlPositionMm(i);

    // Check if motor reached target height
    if (current_mm <= g_initState.target_position_mm) {
      g_initState.complete[i] = true;
      // Bit clear = complete
      g_initState.active_motors &= ~(1 << i);
    } else {
      all_at_target = false;
      // Move down by INIT_PHASE1_STEP_MM
      float next_mm = current_mm - INIT_PHASE1_STEP_MM;
      if (next_mm < g_initState.target_position_mm) {
        next_mm = g_initState.target_position_mm;
      }
      dxlGotoMm(i, next_mm);
    }
  }

  // Transition to Phase 2 once all motors at target height
  if (all_at_target) {
    g_initState.phase = INIT_PHASE2_FINE_DESCENT;
    g_initState.progress_percent = 30;  // Phase 1 complete: 30%
  } else {
    g_initState.progress_percent = 10 + (20 * (1.0f - (float)g_initState.active_motors / 16.0f));
  }
}

// Phase 2: Very slow descent until force target reached
static void updatePhase2() {
  static uint32_t lastPhase2Update = 0;
  const uint32_t PHASE2_UPDATE_INTERVAL_MS = 500;  // Update position every 500ms

  uint32_t now = millis();

  // Always read forces every cycle
  readAllForces();

  // Only update positions every 500ms
  if (now - lastPhase2Update >= PHASE2_UPDATE_INTERVAL_MS) {
    lastPhase2Update = now;

    // Calculate phase 2 step size: descent_rate_mm_per_min / updates_per_minute
    // descent_rate_mm_per_min = 3.33 (10mm/3min)
    // With 500ms updates: updates_per_minute = 120
    // step_mm_per_update = 3.33 / 120 = 0.02775 mm
    float step_mm_per_update = g_initState.descent_rate_mm_per_min / 120.0f;

    bool all_complete = true;
    uint8_t completed_count = 0;

    for (uint8_t i = 0; i < g_dxlCount; i++) {
      if (g_initState.complete[i]) {
        completed_count++;
        continue;
      }

      float current_force = g_force[i];

      // Track peak force
      if (current_force > g_initState.force_peaks[i]) {
        g_initState.force_peaks[i] = current_force;
      }

      // Check if force target reached (mV units from main.cpp readForcemV)
      if (current_force >= (g_forceTarget[i] - FORCE_DEADBAND)) {
        g_initState.complete[i] = true;
        g_initState.active_motors &= ~(1 << i);
        completed_count++;
      } else {
        // Continue descending
        float current_mm = dxlPositionMm(i);
        float next_mm = current_mm - step_mm_per_update;
        dxlGotoMm(i, next_mm);
        all_complete = false;
      }
    }

    // Check if all motors complete or timeout
    if (all_complete) {
      g_initState.phase = INIT_PHASE3_FORCE_HOLD;
      g_initState.progress_percent = 80;
    } else {
      // Phase 2 progress: 30% + (50% * completed_count / dxlCount)
      g_initState.progress_percent = 30 + (50 * completed_count) / g_dxlCount;
    }
  }
}

// Phase 3: Verify force stability
static void updatePhase3() {
  static uint32_t lastPhase3Update = 0;
  const uint32_t PHASE3_UPDATE_INTERVAL_MS = 500;  // Check stability every 500ms

  uint32_t now = millis();

  // Always read forces every cycle
  readAllForces();

  // Only check/adjust positions every 500ms
  if (now - lastPhase3Update >= PHASE3_UPDATE_INTERVAL_MS) {
    lastPhase3Update = now;

    bool all_stable = true;
    uint8_t stable_count = 0;

    for (uint8_t i = 0; i < g_dxlCount; i++) {
      float current_force = g_force[i];
      float target = g_forceTarget[i];

      // Check if force within deadband
      if (current_force >= (target - FORCE_DEADBAND) &&
          current_force <= (target + FORCE_DEADBAND)) {
        stable_count++;
      } else {
        all_stable = false;
        // If force dropped below target, continue descending slightly
        if (current_force < (target - FORCE_DEADBAND)) {
          float current_mm = dxlPositionMm(i);
          float next_mm = current_mm - 0.01f;  // Very tiny step
          dxlGotoMm(i, next_mm);
        }
      }
    }

    // Transition to complete if all stable
    if (all_stable) {
      g_initState.phase = INIT_COMPLETE;
      g_initState.progress_percent = 100;
    } else {
      g_initState.progress_percent = 80 + (20 * stable_count) / g_dxlCount;
    }
  }
}

void initUpdate() {
  if (!g_initRunning) return;

  // Check timeout
  uint32_t elapsed = millis() - g_initState.start_time_ms;
  g_initState.elapsed_ms = elapsed;

  if (elapsed > INIT_MAX_DURATION_MS) {
    g_initState.phase = INIT_ERROR;
    g_initState.error_code = 0x01;  // Timeout
    g_initRunning = false;
    return;
  }

  // State machine
  switch (g_initState.phase) {
    case INIT_PHASE1_DESCENT:
      updatePhase1();
      break;

    case INIT_PHASE2_FINE_DESCENT:
      updatePhase2();
      break;

    case INIT_PHASE3_FORCE_HOLD:
      updatePhase3();
      break;

    case INIT_COMPLETE:
      g_initRunning = false;
      break;

    case INIT_ERROR:
      g_initRunning = false;
      break;

    default:
      break;
  }
}
