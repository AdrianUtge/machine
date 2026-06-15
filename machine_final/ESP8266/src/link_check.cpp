#include "link_check.h"

// Attend une ligne "PONG" sur le port, au plus timeoutMs.
static bool waitForPong(SoftwareSerial& port, uint32_t timeoutMs) {
    char     buf[16];
    size_t   len = 0;
    uint32_t t0  = millis();

    while (millis() - t0 < timeoutMs) {
        while (port.available()) {
            char c = (char)port.read();
            if (c == '\n') {
                buf[len] = '\0';
                if (strcmp(buf, "PONG") == 0) return true;
                len = 0;
            } else if (c == '\r') {
                // ignore
            } else if (len < sizeof(buf) - 1) {
                buf[len++] = c;
            } else {
                len = 0;  // ligne trop longue → corrompue
            }
        }
        yield();
    }
    return false;
}

bool linkCheck(SoftwareSerial& port, uint8_t count, uint32_t timeoutMs, uint32_t warmupMs) {
    // Vide ce qui traine dans le buffer RX
    while (port.available()) port.read();

    // --- Phase 1 : attendre que l'OpenRB soit pret -----------------
    Serial.println("[check] attente OpenRB...");
    uint32_t t0 = millis();
    bool ready = false;
    while (millis() - t0 < warmupMs) {
        port.println("PING");
        if (waitForPong(port, 500)) { ready = true; break; }
    }
    if (!ready) {
        Serial.printf("[check] aucun PONG en %lu ms — OpenRB muet\n",
                      (unsigned long)warmupMs);
        return false;
    }
    Serial.printf("[check] OpenRB pret en %lu ms\n", millis() - t0);

    // Purge les PONG en retard : l'OpenRB a pu repondre en rafale aux
    // PING accumules pendant son boot. On attend un silence sur la ligne
    // (une trame fait ~13 ms @ 4800) puis on vide le buffer.
    uint32_t lastByte = millis();
    while (millis() - lastByte < 50) {
        if (port.available()) { port.read(); lastByte = millis(); }
        yield();
    }

    // --- Phase 2 : serie stricte de `count` echanges ----------------
    uint8_t ok = 0;
    uint32_t tSerie = millis();
    for (uint8_t i = 1; i <= count; i++) {
        port.println("PING");
        if (waitForPong(port, timeoutMs)) {
            ok++;
        } else {
            Serial.printf("[check] echange %u/%u : pas de PONG (timeout %lu ms)\n",
                          i, count, (unsigned long)timeoutMs);
        }
    }
    Serial.printf("[check] %u/%u PONG en %lu ms\n", ok, count, millis() - tSerie);
    return ok == count;
}
