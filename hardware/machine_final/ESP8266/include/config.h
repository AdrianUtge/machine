#pragma once

/**
 * Configuration pour l'ESP8266 — REST API Gateway
 *
 * À configurer en fonction de votre setup:
 * 1. WIFI_SSID: Nom de votre réseau WiFi
 * 2. WIFI_PASSWORD: Mot de passe WiFi
 * 3. AUTH_TOKEN: Clé Bearer pour l'authentification API
 * 4. HTTP_PORT: Port du serveur (par défaut 8080)
 */

// ===== RÉSEAU WIFI =====
#define WIFI_SSID "Your_WiFi_Network"
#define WIFI_PASSWORD "Your_WiFi_Password"

// ===== SÉCURITÉ =====
#define AUTH_TOKEN "your_secret_bearer_token"

// ===== SERVEUR =====
#define HTTP_PORT 8080

// ===== DEBUG =====
#define ENABLE_DEBUG_SERIAL 1
