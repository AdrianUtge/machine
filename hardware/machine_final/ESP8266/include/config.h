#pragma once

/**
 * Configuration pour l'ESP8266 — REST API Gateway
 *
 * À configurer en fonction de votre setup:
 * 1. WIFI_SSID: esp8266_test
 * 2. WIFI_PASSWORD: test_1234
 * 3. AUTH_TOKEN: 1276371237612hj1h12387dsads8912
 * 4. HTTP_PORT: Port du serveur (par défaut 8080)
 */

// ===== RÉSEAU WIFI =====
#define WIFI_SSID "esp8266_test"
#define WIFI_PASSWORD "test_1234"

// ===== SÉCURITÉ =====
#define AUTH_TOKEN "1276371237612hj1h12387dsads8912"

// ===== SERVEUR =====
#define HTTP_PORT 8080

// ===== DEBUG =====
#define ENABLE_DEBUG_SERIAL 1
