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
    // Heure de début de cycle (epoch ms, envoyée par le backend au START).
    // Stockée en String pour renvoyer les chiffres exacts (pas de souci float).
    String cycleStart = "0";
    // Dernières consignes mémorisées (réémises pour reprise après reload).
    float frequency = 0.0f;          // Hz
    float forces[4] = {0, 0, 0, 0};  // force par cellule (N)
} state;

// ===== CORS: ajouter les en-têtes à TOUTES les réponses =====
// Permet au front (navigateur) de joindre directement le NodeMCU.
// Doit être appelé AVANT chaque server.send().
void addCORSHeaders() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
    server.sendHeader("Access-Control-Max-Age", "600");
}

// ===== CORS: répondre aux requêtes préliminaires (preflight OPTIONS) =====
// Le navigateur envoie un OPTIONS avant un POST avec en-têtes custom
// (Authorization / Content-Type). On NE vérifie PAS le token ici :
// les navigateurs n'envoient jamais l'Authorization dans le preflight.
void handleCORSPreflight() {
    addCORSHeaders();
    server.send(204);  // No Content
    Serial.print("[CORS] Preflight OPTIONS handled for: ");
    Serial.println(server.uri());
}

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
    addCORSHeaders();  // CORS sur toutes les réponses (succès comme erreur)

    if (!verifyAuthToken()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }

    Serial.println("[REST] GET /api/status");

    StaticJsonDocument<384> doc;
    doc["status"] = "ok";
    doc["command_count"] = state.commandCount;
    doc["uptime_ms"] = millis();
    doc["rssi"] = WiFi.RSSI();
    doc["version"] = state.version;
    doc["ip"] = WiFi.localIP().toString();
    doc["cycle_start"] = state.cycleStart;   // début de cycle mémorisé (echo)
    doc["frequency"] = state.frequency;      // consigne fréquence mémorisée
    JsonArray forcesArr = doc.createNestedArray("forces");  // consignes force par cellule
    for (int i = 0; i < 4; i++) forcesArr.add(state.forces[i]);

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);

    Serial.println("[REST] Status sent");
}

// ===== Endpoint: POST /api/command =====
void handleCommand() {
    addCORSHeaders();  // CORS sur toutes les réponses (succès comme erreur)

    if (!verifyAuthToken()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }

    Serial.println("[REST] POST /api/command — auth OK, traitement de la commande");

    if (server.method() != HTTP_POST) {
        Serial.println("[REST] ❌ Méthode non autorisée");
        server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
        return;
    }

    // NB: on ne teste PAS hasHeader("Content-Type") : l'ESP8266WebServer ne
    // collecte par défaut que les en-têtes Authorization et ETag, donc ce test
    // renvoyait toujours un 400 et la commande n'était jamais loggée.
    String body = server.arg("plain");
    Serial.print("[REST] Corps reçu: ");
    Serial.println(body);

    if (body.length() == 0) {
        Serial.println("[REST] ❌ Corps vide (pas de JSON)");
        server.send(400, "application/json", "{\"error\":\"Empty body\"}");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        Serial.println("[REST] ❌ Invalid JSON");
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    if (!doc.containsKey("command")) {
        Serial.println("[REST] ❌ Missing command field");
        server.send(400, "application/json", "{\"error\":\"Missing command field\"}");
        return;
    }

    String command = doc["command"].as<String>();
    command.toUpperCase();

    // Commands: START, STOP, HOME, FREQUENCY, SPEED, FORCE, PRESET, MANUAL
    const char* validCommands[] = {
        "START", "STOP", "HOME", "HARD_RESET",
        "FREQUENCY", "SPEED", "FORCE", "GOTO", "PRESET", "MANUAL", "STATUS"
    };
    const int validCommandsCount = sizeof(validCommands) / sizeof(validCommands[0]);

    bool isValid = false;
    for (int i = 0; i < validCommandsCount; i++) {
        if (command == validCommands[i]) {
            isValid = true;
            break;
        }
    }

    if (!isValid) {
        Serial.print("[CMD] ❌ Unknown command: ");
        Serial.println(command);
        server.send(400, "application/json", "{\"error\":\"Unknown command\"}");
        return;
    }

    // Cycle start time: mémoriser au START (renvoyé ensuite dans /api/status),
    // remettre à zéro au STOP / HARD_RESET.
    if (command == "START" && doc.containsKey("start_time")) {
        state.cycleStart = doc["start_time"].as<String>();
    } else if (command == "STOP" || command == "HARD_RESET") {
        state.cycleStart = "0";
    }

    // Mémoriser les consignes pour pouvoir les réémettre (reprise après reload)
    if (command == "FREQUENCY" && doc.containsKey("frequency")) {
        state.frequency = doc["frequency"].as<float>();
    }
    if (command == "FORCE" && doc.containsKey("force")) {
        float f = doc["force"].as<float>();
        if (doc.containsKey("sensor")) {
            int s = doc["sensor"].as<int>();
            if (s >= 1 && s <= 4) state.forces[s - 1] = f;
        } else {
            for (int i = 0; i < 4; i++) state.forces[i] = f;  // global = les 4
        }
    }

    // Log the command
    state.commandCount++;
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.print("║ [CMD #");
    Serial.print(state.commandCount);
    Serial.println("] Command received");
    Serial.print("║ Command: ");
    Serial.println(command);

    // Log parameters if provided
    if (doc.containsKey("frequency")) {
        Serial.print("║ Frequency: ");
        Serial.print(doc["frequency"].as<float>());
        Serial.println(" Hz");
    }
    if (doc.containsKey("speed")) {
        Serial.print("║ Speed: ");
        Serial.print(doc["speed"].as<int>());
        Serial.println(" %");
    }
    if (doc.containsKey("force")) {
        Serial.print("║ Force: ");
        Serial.print(doc["force"].as<float>());
        Serial.println(" N");
    }
    if (doc.containsKey("sensor")) {
        Serial.print("║ Cell (sensor): ");
        Serial.println(doc["sensor"].as<int>());
    }
    if (doc.containsKey("table")) {
        Serial.print("║ Table: ");
        Serial.println(doc["table"].as<int>());
    }
    if (doc.containsKey("position")) {
        Serial.print("║ Position: ");
        Serial.print(doc["position"].as<float>());
        Serial.println(" mm");
    }
    if (doc.containsKey("preset")) {
        Serial.print("║ Preset: ");
        Serial.println(doc["preset"].as<String>());
    }
    if (doc.containsKey("command_data")) {
        Serial.print("║ Data: ");
        Serial.println(doc["command_data"].as<String>());
    }

    Serial.print("║ Timestamp: ");
    Serial.print(millis());
    Serial.println(" ms");
    Serial.println("╚════════════════════════════════════════╝\n");

    // TODO: Phase 2 - Envoyer à l'OpenRB via UART
    // Pour l'instant, juste logger

    // Répondre avec succès
    StaticJsonDocument<256> response;
    response["result"] = "success";
    response["command"] = command;
    response["timestamp"] = millis();
    response["command_number"] = state.commandCount;

    String responseStr;
    serializeJson(response, responseStr);
    server.send(200, "application/json", responseStr);

    Serial.println("[REST] ✅ Response sent");
}

// ===== Gestion des requêtes non trouvées =====
void handleNotFound() {
    // Répondre aux preflight CORS même sur des routes inconnues (filet de sécurité)
    if (server.method() == HTTP_OPTIONS) {
        handleCORSPreflight();
        return;
    }

    addCORSHeaders();  // CORS aussi sur les 404
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
    server.on("/api/status", HTTP_OPTIONS, handleCORSPreflight);   // preflight CORS
    server.on("/api/command", HTTP_POST, handleCommand);
    server.on("/api/command", HTTP_OPTIONS, handleCORSPreflight);  // preflight CORS
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
