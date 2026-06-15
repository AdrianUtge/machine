#include <Arduino.h>

// OpenRB-150 — USB Serial → Toggle GPIO0

#define PIN_OUT  0

// --- Setup ----------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_OUT, OUTPUT);
  digitalWrite(PIN_OUT, LOW);
  Serial.println("Ready — send 'H' or 'L'");
}

// --- Loop -----------------------------------------------------------------
void loop() {
  if (Serial.available()) {
    char c = Serial.read();

    if (c == 'H' || c == 'h') {
      digitalWrite(PIN_OUT, HIGH);
      Serial.println("GPIO0 → HIGH");
    }
    else if (c == 'L' || c == 'l') {
      digitalWrite(PIN_OUT, LOW);
      Serial.println("GPIO0 → LOW");
    }
  }
}