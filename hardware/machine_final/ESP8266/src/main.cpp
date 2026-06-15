/**
 * NodeMCU v3 (ESP8266) — test générateur de niveau pour l'ADC OpenRB
 *
 * Sketch minimal : commandes HIGH / LOW sur le moniteur USB → la broche
 * D0 passe a 3.3 V ou 0 V. A cabler sur l'entree A1 de l'OpenRB-150
 * (+ GND commun) pour verifier la chaine ADC (burst ~4095 ou ~0).
 *
 * Le code complet du banc est sauvegarde dans main_banc.cpp.bak.
 *
 * Commandes (moniteur USB @ 115200) :
 *  HIGH    D0 → 3.3 V
 *  LOW     D0 → 0 V
 *  STATUS  etat courant de D0
 */

#include <Arduino.h>

static const uint8_t PIN_OUT = D0;   // GPIO16 → vers A1 OpenRB
static const uint8_t PIN_LED = D4;   // GPIO2 = LED module (active LOW)

static char   buf[16];
static size_t len = 0;

static void handleLine(const char* line) {
    if (strcasecmp(line, "HIGH") == 0) {
        digitalWrite(PIN_OUT, HIGH);
        digitalWrite(PIN_LED, LOW);    // LED allumee = sortie haute
        Serial.println("D0 (GPIO16) → HIGH (3.3 V)");
    } else if (strcasecmp(line, "LOW") == 0) {
        digitalWrite(PIN_OUT, LOW);
        digitalWrite(PIN_LED, HIGH);
        Serial.println("D0 (GPIO16) → LOW (0 V)");
    } else if (strcasecmp(line, "STATUS") == 0) {
        Serial.printf("D0 = %s\n", digitalRead(PIN_OUT) ? "HIGH" : "LOW");
    } else if (line[0] != '\0') {
        Serial.println("Commandes : HIGH | LOW | STATUS");
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);

    pinMode(PIN_OUT, OUTPUT);
    digitalWrite(PIN_OUT, LOW);
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);

    Serial.println("\n=== NodeMCU — test niveau D0 → A1 OpenRB ===");
    Serial.println("D0 = LOW au boot. Commandes : HIGH | LOW | STATUS");
}

void loop() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n') {
            buf[len] = '\0';
            handleLine(buf);
            len = 0;
        } else if (c == '\r') {
            // ignore
        } else if (len < sizeof(buf) - 1) {
            buf[len++] = c;
        } else {
            len = 0;
        }
    }
}
