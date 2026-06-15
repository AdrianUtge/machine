#include <Arduino.h>

// put function declarations here:
int myFunction(int, int);
#define PUL_PIN 6 
#define DIR_PIN 7
#define EN_PIN 8

float F = 100.0; // frequency in Hz



void setup() {
  // put your setup code here, to run once:

  pinMode(PUL_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);

  digitalWrite(EN_PIN, LOW); // Enable the driver
  digitalWrite(DIR_PIN, LOW); // Set direction

  tone(PUL_PIN, F); // Start generating pulses at the specified frequency
}

void loop() {
  // put your main code here, to run repeatedly:
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}