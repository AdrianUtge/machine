#pragma once

/**
 * ===========================================================================
 * FILE: config.h
 * ROLE:
 *   Configuration de l'ESP8266 (passerelle REST en mode Access Point).
 *
 * SOURCE DES SECRETS:
 *   Les valeurs WIFI_SSID / WIFI_PASSWORD / AUTH_TOKEN / HTTP_PORT proviennent
 *   de `include/secrets.h`, AUTO-GÉNÉRÉ au build par `gen_secrets.py` à partir
 *   de `.machine_config.ini` (racine PROD/, source unique, gitignorée).
 *
 *   secrets.h est gitignoré. S'il est absent (premier build sans .ini), on
 *   retombe sur les valeurs par défaut "usine" définies plus bas -> le firmware
 *   compile toujours.
 *
 * IMPORTANT:
 *   Le jeton AUTH_TOKEN doit être IDENTIQUE à [nodemcu].key du .machine_config.ini
 *   côté backend. Après toute modif du .ini : `pio run` régénère secrets.h, puis
 *   RE-FLASHER l'ESP8266.
 * ===========================================================================
 */

// Inclut le header de secrets généré s'il existe (sinon : valeurs par défaut).
#if defined(__has_include)
#  if __has_include("secrets.h")
#    include "secrets.h"
#  endif
#endif

// ===== Valeurs par défaut (fallback si secrets.h absent) =====
#ifndef SECRET_WIFI_SSID
#  define SECRET_WIFI_SSID "NodeMCU-Control"
#endif
#ifndef SECRET_WIFI_PASSWORD
#  define SECRET_WIFI_PASSWORD "12345678"
#endif
#ifndef SECRET_AUTH_TOKEN
#  define SECRET_AUTH_TOKEN "change_me_bearer_token"
#endif
#ifndef SECRET_HTTP_PORT
#  define SECRET_HTTP_PORT 8080
#endif

// ===== RÉSEAU WIFI (créé par l'ESP8266, mode Access Point) =====
#define WIFI_SSID     SECRET_WIFI_SSID       // Nom du réseau WiFi diffusé
#define WIFI_PASSWORD SECRET_WIFI_PASSWORD   // Mot de passe du réseau

// ===== SÉCURITÉ =====
#define AUTH_TOKEN    SECRET_AUTH_TOKEN      // Jeton Bearer pour l'API REST

// ===== SERVEUR =====
#define HTTP_PORT     SECRET_HTTP_PORT       // Port du serveur HTTP (défaut 8080)

// ===== DEBUG =====
#define ENABLE_DEBUG_SERIAL 1
