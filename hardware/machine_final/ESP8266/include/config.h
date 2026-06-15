#pragma once

/**
 * Configuration pour l'ESP8266 — REST API Gateway (AP Mode)
 *
 * L'ESP8266 crée son propre réseau WiFi (Access Point)
 * Les clients se connectent AU réseau créé par l'ESP8266
 *
 * À personnaliser:
 * 1. WIFI_SSID: Nom du réseau broadcast par l'ESP8266
 * 2. WIFI_PASSWORD: Mot de passe du réseau ESP8266
 * 3. AUTH_TOKEN: Token Bearer pour l'API REST
 * 4. HTTP_PORT: Port du serveur HTTP (défaut 8080)
 */

// ===== RÉSEAU WIFI (créé par l'ESP8266) =====
#define WIFI_SSID "NodeMCU-Control"          // Nom du réseau WiFi
#define WIFI_PASSWORD "12345678"             // Mot de passe du réseau

// ===== SÉCURITÉ =====
#define AUTH_TOKEN "bearer_token_secret"     // Token pour API REST

// ===== SERVEUR =====
#define HTTP_PORT 8080                       // Port du serveur HTTP

// ===== DEBUG =====
#define ENABLE_DEBUG_SERIAL 1
