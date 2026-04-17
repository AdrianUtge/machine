#include "HX711.h"

#define DT 6
#define SCK 9

HX711 scale;

long offset = 0;
bool calibrated = false;

// --- Serial helpers --------------------------------------------------------

String readLine() {
    static String buffer = "";

    while (Serial.available() > 0) {
        char c = Serial.read();

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            String line = buffer;
            buffer = "";
            line.trim();
            return line;
        }

        buffer += c;
    }

    return "";
}

bool isYes(const String& input) {
    String s = input;
    s.trim();
    s.toLowerCase();

    return (s == "oui" || s == "o" || s == "yes" || s == "y");
}

// --- HX711 -----------------------------------------------------------------

float readForceRawAverage(int samples = 10) {
    long raw = scale.read_average(samples);
    return raw - offset;
}

// --- Main ------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    scale.begin(DT, SCK);

    Serial.println();
    Serial.println("Initialisation HX711...");

    while (!scale.is_ready()) {
        Serial.println("HX711 non pret...");
        delay(300);
    }

    Serial.println("HX711 pret.");
    Serial.println("La cellule n'est pas contrainte ?");
    Serial.println("Tape 'oui' puis entree pour prendre l'offset.");
}

void loop() {
    if (!calibrated) {
        String cmd = readLine();

        if (cmd.length() > 0) {
            if (isYes(cmd)) {
                Serial.println("Prise d'offset en cours...");
                delay(1000);

                offset = scale.read_average(20);

                Serial.print("Offset = ");
                Serial.println(offset);

                calibrated = true;
                Serial.println("Calibration offset terminee.");
                Serial.println("Lecture continue lancee.");
            } else {
                Serial.println("Reponse non reconnue. Tape 'oui' puis entree.");
            }
        }

        return;
    }

    if (scale.is_ready()) {
        float value = readForceRawAverage(10);

        Serial.print("Valeur brute corrigee = ");
        Serial.println(value);
    } else {
        Serial.println("HX711 non pret");
    }

    delay(200);
}