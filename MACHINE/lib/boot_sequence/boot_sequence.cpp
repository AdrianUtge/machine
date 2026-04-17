#include <Arduino.h>
#include "boot_sequence.h"


void boot_sequence() {
    Serial.println("Booting...");
    delay(500);
    Serial.println("Boot OK");
}
