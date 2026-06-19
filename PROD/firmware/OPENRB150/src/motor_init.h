/**
 * motor_init.h — Motor initialization / force calibration state machine
 *
 * Three-phase process to calibrate motor positions and lock at target force:
 * 1. PHASE1_DESCENT: Fast descent to predetermined height (e.g., 50mm)
 * 2. PHASE2_FINE_DESCENT: Very slow descent (10mm/3min) until force target reached
 * 3. PHASE3_FORCE_HOLD: Motors locked at target force, stabilize within deadband
 *
 * Blocking process: max 120 seconds total
 * Called from main loop every 50ms (same interval as force control loop)
 */

#ifndef MOTOR_INIT_H
#define MOTOR_INIT_H

#include <Arduino.h>

enum InitPhase {
  INIT_IDLE = 0,
  INIT_PHASE1_DESCENT = 1,
  INIT_PHASE2_FINE_DESCENT = 2,
  INIT_PHASE3_FORCE_HOLD = 3,
  INIT_COMPLETE = 4,
  INIT_ERROR = 5
};

struct InitState {
  InitPhase phase;
  uint32_t start_time_ms;         // When init started (millis())
  float target_position_mm;        // Depth for Phase 1 (e.g., 50.0)
  float descent_rate_mm_per_min;   // Phase 2 speed (e.g., 3.33 = 10mm/3min)
  uint8_t active_motors;           // Bitmask: which motors still descending (1=active, 0=complete)
  float force_peaks[4];            // Peak force during this init cycle per motor
  bool complete[4];                // Which motors reached force target
  uint32_t elapsed_ms;             // Total elapsed time
  uint16_t error_code;             // 0 = success, nonzero = error code
  uint8_t progress_percent;        // 0-100% for UI
};

// Global init state (extern in main.cpp)
extern InitState g_initState;
extern bool g_initRunning;

// Function signatures
void initStart(float target_position_mm, float descent_rate_mm_per_min);
void initStop();
void initUpdate();  // Called from main loop at 50ms intervals
bool initIsRunning();
InitState initGetState();

#endif // MOTOR_INIT_H
