/*
  HX711 Load Cell Multi-Point Calibration + Teleplot Stream

  Commands:
  - tare          : reset zero offset
  - cal 1000     : record calibration point with 1000 g placed on load cell
  - cal 2000     : record calibration point with 2000 g placed on load cell
  - fit          : compute linear calibration equation
  - read         : single reading
  - stream       : start Teleplot stream
  - stop         : stop Teleplot stream
  - clear        : clear calibration points

  Teleplot output:
  >raw:value
  >force_N:value
  >mass_g:value
*/

#include "HX711.h"

// --- Pins ---------------------------------------------------------------

#define HX711_DT  6
#define HX711_SCK 9

// --- Constants ----------------------------------------------------------

const float G = 9.81;

const int MAX_CAL_POINTS = 10;

const int ZERO_SAMPLES = 100;
const int CAL_SAMPLES = 150;
const int READ_SAMPLES = 30;
const int STREAM_SAMPLES = 10;

const unsigned long STREAM_PERIOD_MS = 100;

// --- Objects ------------------------------------------------------------

HX711 scale;

// --- State --------------------------------------------------------------

long raw_zero = 0;

bool streaming = true;
bool calibrated = false;

unsigned long lastStreamTime = 0;

// Equation:
// F[N] = a * raw + b
float a_force = 0.0;
float b_force = 0.0;

// Calibration arrays
float cal_force_N[MAX_CAL_POINTS];
long cal_raw[MAX_CAL_POINTS];
int cal_count = 0;

// --- Helpers ------------------------------------------------------------

long readAverageRaw(int samples) {
  long sum = 0;

  for (int i = 0; i < samples; i++) {
    while (!scale.is_ready()) {
      delay(1);
    }

    sum += scale.read();
    delay(10);
  }

  return sum / samples;
}

float rawToForce(long raw) {
  return a_force * raw + b_force;
}

float forceToMassG(float force_N) {
  return (force_N / G) * 1000.0;
}

void printTeleplot(long raw, float force_N, float mass_g) {
  Serial.print(">raw:");
  Serial.println(raw);

  Serial.print(">force_N:");
  Serial.println(force_N, 6);

  Serial.print(">mass_g:");
  Serial.println(mass_g, 3);
}

void printHuman(long raw, float force_N, float mass_g) {
  Serial.print("raw = ");
  Serial.print(raw);

  Serial.print(" | F = ");
  Serial.print(force_N, 6);
  Serial.print(" N");

  Serial.print(" | m = ");
  Serial.print(mass_g, 3);
  Serial.println(" g");
}

void tareLoadCell() {
  streaming = false;

  Serial.println("Tare started...");
  Serial.println("Remove all load from the load cell.");
  delay(1500);

  raw_zero = readAverageRaw(ZERO_SAMPLES);

  Serial.print("Raw zero = ");
  Serial.println(raw_zero);

  streaming = true;

  Serial.println("DONE:TARE");
}

void addCalibrationPoint(float mass_g) {
  if (cal_count >= MAX_CAL_POINTS) {
    Serial.println("ERROR:TOO_MANY_CAL_POINTS");
    return;
  }

  streaming = false;

  float mass_kg = mass_g / 1000.0;
  float force_N = mass_kg * G;

  Serial.println();
  Serial.print("Calibration point started for ");
  Serial.print(mass_g, 3);
  Serial.println(" g");

  Serial.println("Place the weight on the load cell.");
  delay(1500);

  long raw = readAverageRaw(CAL_SAMPLES);

  cal_raw[cal_count] = raw;
  cal_force_N[cal_count] = force_N;
  cal_count++;

  Serial.print("Point added: raw = ");
  Serial.print(raw);
  Serial.print(" | mass = ");
  Serial.print(mass_g, 3);
  Serial.print(" g");
  Serial.print(" | force = ");
  Serial.print(force_N, 6);
  Serial.println(" N");

  Serial.print("Calibration points = ");
  Serial.println(cal_count);

  streaming = true;

  Serial.println("DONE:CAL_POINT");
}

void clearCalibrationPoints() {
  cal_count = 0;
  calibrated = false;
  a_force = 0.0;
  b_force = 0.0;

  Serial.println("Calibration points cleared.");
  Serial.println("DONE:CLEAR");
}

void fitCalibration() {
  if (cal_count < 1) {
    Serial.println("ERROR:NOT_ENOUGH_POINTS");
    Serial.println("Use at least one point: cal 1000");
    return;
  }

  // Add tare point automatically: raw_zero -> 0 N
  int n = cal_count + 1;

  double sum_x = raw_zero;
  double sum_y = 0.0;
  double sum_xx = (double)raw_zero * (double)raw_zero;
  double sum_xy = 0.0;

  for (int i = 0; i < cal_count; i++) {
    double x = (double)cal_raw[i];
    double y = (double)cal_force_N[i];

    sum_x += x;
    sum_y += y;
    sum_xx += x * x;
    sum_xy += x * y;
  }

  double denom = n * sum_xx - sum_x * sum_x;

  if (abs(denom) < 1e-9) {
    Serial.println("ERROR:FIT_FAILED");
    Serial.println("All raw values are too close.");
    return;
  }

  a_force = (n * sum_xy - sum_x * sum_y) / denom;
  b_force = (sum_y - a_force * sum_x) / n;

  calibrated = true;

  Serial.println();
  Serial.println("----- Calibration fit result -----");

  Serial.println("Equation:");
  Serial.print("F[N] = ");
  Serial.print(a_force, 12);
  Serial.print(" * raw + ");
  Serial.println(b_force, 12);

  Serial.println();

  Serial.println("Mass equation:");
  Serial.print("m[g] = ((");
  Serial.print(a_force, 12);
  Serial.print(" * raw + ");
  Serial.print(b_force, 12);
  Serial.println(") / 9.81) * 1000");

  Serial.println();

  Serial.println("Calibration points used:");
  Serial.print("tare: raw = ");
  Serial.print(raw_zero);
  Serial.println(" | F = 0 N");

  for (int i = 0; i < cal_count; i++) {
    Serial.print("point ");
    Serial.print(i + 1);
    Serial.print(": raw = ");
    Serial.print(cal_raw[i]);
    Serial.print(" | F = ");
    Serial.print(cal_force_N[i], 6);
    Serial.println(" N");
  }

  Serial.println("----------------------------------");
  Serial.println("DONE:FIT");
}

void readOnce() {
  if (!calibrated) {
    Serial.println("ERROR:NOT_CALIBRATED");
    Serial.println("Use: tare, cal 1000, cal 2000, fit");
    return;
  }

  long raw = readAverageRaw(READ_SAMPLES);

  float force_N = rawToForce(raw);
  float mass_g = forceToMassG(force_N);

  printHuman(raw, force_N, mass_g);
  printTeleplot(raw, force_N, mass_g);

  Serial.println("DONE:READ");
}

void streamOnce() {
  long raw = readAverageRaw(STREAM_SAMPLES);

  if (!calibrated) {
    Serial.print(">raw:");
    Serial.println(raw);
    return;
  }

  float force_N = rawToForce(raw);
  float mass_g = forceToMassG(force_N);

  printTeleplot(raw, force_N, mass_g);
}

float parseMassFromCommand(String cmd) {
  cmd.replace("cal", "");
  cmd.trim();
  return cmd.toFloat();
}

// --- Setup --------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(1000);

  scale.begin(HX711_DT, HX711_SCK);

  Serial.println("HX711 Multi-Point Calibration");
  Serial.println("--------------------------------");
  Serial.println("Stream: ON by default");
  Serial.println();

  Serial.println("Initial tare in 2 seconds...");
  Serial.println("Remove all load from the load cell.");
  delay(2000);

  raw_zero = readAverageRaw(ZERO_SAMPLES);

  Serial.print("Raw zero = ");
  Serial.println(raw_zero);

  Serial.println();
  Serial.println("Commands:");
  Serial.println("  tare       -> reset zero");
  Serial.println("  cal 1000   -> add point with 1000 g");
  Serial.println("  cal 2000   -> add point with 2000 g");
  Serial.println("  fit        -> compute equation");
  Serial.println("  read       -> single reading");
  Serial.println("  stream     -> start stream");
  Serial.println("  stop       -> stop stream");
  Serial.println("  clear      -> clear calibration");
  Serial.println();

  Serial.println("Recommended for 50 N sensor:");
  Serial.println("  tare");
  Serial.println("  cal 1000");
  Serial.println("  cal 2000");
  Serial.println("  fit");
  Serial.println();
}

// --- Loop ---------------------------------------------------------------

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "tare") {
      tareLoadCell();
    }

    else if (cmd.startsWith("cal")) {
      float mass_g = parseMassFromCommand(cmd);

      if (mass_g <= 0.0) {
        Serial.println("ERROR:INVALID_MASS");
        Serial.println("Example: cal 1000");
      } else {
        addCalibrationPoint(mass_g);
      }
    }

    else if (cmd == "fit") {
      fitCalibration();
    }

    else if (cmd == "read") {
      readOnce();
    }

    else if (cmd == "stream") {
      streaming = true;
      Serial.println("DONE:STREAM");
    }

    else if (cmd == "stop") {
      streaming = false;
      Serial.println("DONE:STOP");
    }

    else if (cmd == "clear") {
      clearCalibrationPoints();
    }

    else {
      Serial.println("ERROR:UNKNOWN_COMMAND");
      Serial.println("Use: tare, cal 1000, cal 2000, fit, read, stream, stop, clear");
    }
  }

  if (streaming && millis() - lastStreamTime >= STREAM_PERIOD_MS) {
    lastStreamTime = millis();
    streamOnce();
  }
}