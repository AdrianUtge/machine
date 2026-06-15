/**
 * NodeMCU v3 (ESP8266) — WiFi REST API Gateway
 *
 * Phase 1: Simple logging via USB
 * Fonction : Serveur HTTP exposant une API REST pour recevoir des commandes
 * via WiFi. Les commandes sont loggées dans le port USB (Serial).
 *
 * Phase 2 (future): Communication avec OpenRB-150
 * Les commandes seront ensuite envoyées à l'OpenRB via UART/SoftwareSerial
 *
 * Endpoints:
 *  GET  /api/status  → État actuel du système
 *  POST /api/command → Recevoir une commande (ex: {"command":"HIGH"})
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include "config.h"

// ===== Hardware =====
static const uint8_t PIN_LED = D4;      // GPIO2 = LED intégrée (actif bas)

// ===== Serveur Web =====
ESP8266WebServer server(HTTP_PORT);

// ===== État du système =====
struct SystemState {
    uint32_t commandCount = 0;
    uint32_t uptime = 0;
    int rssi = 0;
    const char* version = "1.0.0-phase1";
} state;

// ===== Fonction auxiliaire: Vérifier le token Bearer =====
bool verifyAuthToken() {
    if (!server.hasHeader("Authorization")) {
        Serial.println("[AUTH] ❌ Missing Authorization header");
        return false;
    }

    String authHeader = server.header("Authorization");
    String expectedAuth = String("Bearer ") + String(AUTH_TOKEN);

    Serial.print("[AUTH] Received: ");
    Serial.println(authHeader);
    Serial.print("[AUTH] Expected: ");
    Serial.println(expectedAuth);

    bool valid = (authHeader == expectedAuth);

    if (!valid) {
        Serial.println("[AUTH] ❌ Token mismatch!");
        Serial.print("[AUTH] Received length: ");
        Serial.print(authHeader.length());
        Serial.print(" vs Expected length: ");
        Serial.println(expectedAuth.length());
    } else {
        Serial.println("[AUTH] ✅ Token valid");
    }

    return valid;
}

// ===== Endpoint: GET /api/status =====
void handleStatus() {
    if (!verifyAuthToken()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }

    Serial.println("[REST] GET /api/status");

    StaticJsonDocument<256> doc;
    doc["status"] = "ok";
    doc["command_count"] = state.commandCount;
    doc["uptime_ms"] = millis();
    doc["rssi"] = WiFi.RSSI();
    doc["version"] = state.version;
    doc["ip"] = WiFi.localIP().toString();

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);

    Serial.println("[REST] Status sent");
}

// ===== Endpoint: POST /api/command =====
void handleCommand() {
    if (!verifyAuthToken()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }

    if (server.method() != HTTP_POST) {
        server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
        return;
    }

    if (!server.hasHeader("Content-Type")) {
        server.send(400, "application/json", "{\"error\":\"Missing Content-Type\"}");
        return;
    }

    String body = server.arg("plain");
    Serial.print("[REST] Received command: ");
    Serial.println(body);

    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        Serial.println("[REST] ERROR: Invalid JSON");
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    if (!doc.containsKey("command")) {
        Serial.println("[REST] ERROR: Missing command field");
        server.send(400, "application/json", "{\"error\":\"Missing command field\"}");
        return;
    }

    String command = doc["command"].as<String>();
    command.toUpperCase();

    Serial.print("[CMD] Processing: ");
    Serial.println(command);

    // Valider la commande
    if (command != "HIGH" && command != "LOW" && command != "STATUS") {
        Serial.println("[CMD] ERROR: Unknown command");
        server.send(400, "application/json", "{\"error\":\"Unknown command\"}");
        return;
    }

    // TODO: Phase 2 - Envoyer à l'OpenRB via UART
    // Pour l'instant, juste logger
    state.commandCount++;
    Serial.print("[LOG] Command #");
    Serial.print(state.commandCount);
    Serial.print(": ");
    Serial.println(command);

    // Répondre avec succès
    StaticJsonDocument<256> response;
    response["result"] = "success";
    response["command"] = command;
    response["timestamp"] = millis();
    response["command_number"] = state.commandCount;

    String responseStr;
    serializeJson(response, responseStr);
    server.send(200, "application/json", responseStr);

    Serial.println("[REST] Command response sent");
}

// ===== Gestion des requêtes non trouvées =====
void handleNotFound() {
    Serial.print("[HTTP] 404 - Path: ");
    Serial.println(server.uri());
    server.send(404, "application/json",
        "{\"error\":\"Not found\",\"path\":\"" + server.uri() + "\"}");
}

// ===== Initialisation WiFi (AP Mode) =====
bool setupWiFi() {
    Serial.print("\n[WiFi] Starting Access Point: ");
    Serial.println(WIFI_SSID);

    // Mode AP (Access Point) - l'ESP8266 broadcast son propre réseau
    WiFi.mode(WIFI_AP);
    bool apStarted = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

    if (!apStarted) {
        Serial.println("[WiFi] ERROR: Failed to start AP!");
        return false;
    }

    Serial.println("[WiFi] ✓ Access Point started!");
    Serial.print("[WiFi] SSID: ");
    Serial.println(WIFI_SSID);
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("[WiFi] Gateway: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("[WiFi] Password: " WIFI_PASSWORD);

    return true;
}

// ===== Initialisation du serveur HTTP =====
void setupServer() {
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/command", HTTP_POST, handleCommand);
    server.onNotFound(handleNotFound);

    server.begin();
    Serial.print("[Server] HTTP server started on port ");
    Serial.println(HTTP_PORT);
}

// ===== Setup principal =====
void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println("\n\n╔════════════════════════════════════════╗");
    Serial.println("║  NodeMCU ESP8266 — WiFi REST Gateway   ║");
    Serial.println("║  Phase 1: Command Logging Only         ║");
    Serial.println("╚════════════════════════════════════════╝");

    // Initialiser LED
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);   // LED off (actif bas)

    // WiFi
    if (!setupWiFi()) {
        Serial.println("\n[FATAL] WiFi connection failed!");
        Serial.println("[FATAL] Blinking LED - check WiFi settings");
        while (true) {
            digitalWrite(PIN_LED, !digitalRead(PIN_LED));
            delay(200);
        }
    }

    // Serveur HTTP
    setupServer();

    Serial.println("\n[Boot] ✓ Ready to receive commands via REST API");
    Serial.println("[Boot] ═════════════════════════════════════════");
    Serial.print("[Boot] SSID: ");
    Serial.println(WIFI_SSID);
    Serial.print("[Boot] Password: ");
    Serial.println(WIFI_PASSWORD);
    Serial.print("[Boot] IP: http://");
    Serial.print(WiFi.softAPIP());
    Serial.print(":");
    Serial.println(HTTP_PORT);
    Serial.print("[Boot] Auth Token: ");
    Serial.println(AUTH_TOKEN);
    Serial.println("[Boot] ═════════════════════════════════════════");
    Serial.println("[Boot] Connect your PC to this WiFi network");
    Serial.println("[Boot] Then access the REST API at the IP above");
    Serial.println("[Boot] Use the Auth Token above for API requests");
    Serial.println("[Boot] All commands will be logged here\n");
}

// ===== Loop principal =====
void loop() {
    // Traiter les requêtes HTTP
    server.handleClient();

    // LED status indicator
    static uint32_t lastBlink = 0;
    if (millis() - lastBlink > 1000) {
        digitalWrite(PIN_LED, !digitalRead(PIN_LED));  // Toggle LED
        lastBlink = millis();
    }
}
