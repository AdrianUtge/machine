#pragma once
#include <Arduino.h>
#include <SoftwareSerial.h>

// Auto-test de la liaison OpenRB au boot.
//
// 1. Phase d'attente : PING repetes jusqu'au premier PONG (l'OpenRB peut
//    booter jusqu'a ~3 s apres l'ESP a cause de son attente USB).
// 2. Test strict : `count` echanges PING/PONG, chacun avec `timeoutMs`
//    pour repondre (5 x 200 ms = la serie passe en moins d'une seconde).
//
// Retourne true si les `count` PONG sont tous revenus.
bool linkCheck(SoftwareSerial& port,
               uint8_t  count     = 5,
               uint32_t timeoutMs = 200,
               uint32_t warmupMs  = 5000);
