/**
 * Debug sniffer — Arduino Uno
 * Écoute le TX SoftwareSerial du NodeMCU v3 et affiche sur le monitor USB
 *
 * Câblage :
 *  NodeMCU D6 (GPIO12, TX SoftwareSerial) → Uno pin 10 (RX)
 *  NodeMCU D5 (GPIO14, RX SoftwareSerial) ← Uno pin 11 (TX) via diviseur tension !
 *  GND NodeMCU → GND Uno
 *
 * Note : le NodeMCU sort du 3.3V. L'Uno lit en 5V mais 3.3V
 * est généralement reconnu comme HIGH — ça fonctionne en pratique.
 * Si signal instable, ajoute une résistance pull-up 10k sur pin 10.
 */

#include <Arduino.h>
#include <SoftwareSerial.h>

// Pin 10 = RX (écoute le TX du NodeMCU)
// Pin 11 = TX (inutilisé ici)
SoftwareSerial espSerial(10, 11);

#define ESP_BAUD 4800  // doit correspondre à OPENRB_BAUD dans le code NodeMCU

void setup() {
    Serial.begin(115200);
    espSerial.begin(ESP_BAUD);
    Serial.println("=== Uno debug sniffer — NodeMCU TX ===");
    Serial.print("Ecoute sur pin 10 @ ");
    Serial.print(ESP_BAUD);
    Serial.println(" baud");
    Serial.println("--------------------------------------");
}

void loop() {
    // Affiche ce qui arrive du NodeMCU
    while (espSerial.available()) {
        Serial.print((char)espSerial.read());
    }

    // Envoie PONG toutes les 2 secondes
    static unsigned long lastPong = 0;
    if (millis() - lastPong >= 2000) {
        lastPong = millis();
        espSerial.println("PONG");
        Serial.println("[TX] PONG");
    }
}
