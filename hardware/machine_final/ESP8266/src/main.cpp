/**
 * NodeMCU v3 (ESP8266) — WiFi REST API Gateway
 *
 * Fonction : Serveur HTTP exposant une API REST pour contrôler l'OpenRB-150
 * via WiFi. Le frontend envoie des requêtes HTTP, l'ESP8266 les traduit en
 * commandes série et les envoie à l'OpenRB-150.
 *
 * Endpoints:
 *  GET  /api/status  → État actuel du système
 *  POST /api/command → Envoyer une commande (ex: {"command":"HIGH"})
 *
 * Configuration: À définir dans setup.json sur le backend
 *  - WiFi SSID & password
 *  - Adresse IP & port de l'ESP
 *  - Bearer token d'authentification
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include "config.h"

// ===== Configuration (depuis config.h) =====
// À personnaliser dans include/config.h

// ===== Hardware =====
static const uint8_t PIN_OUT = D0;      // GPIO16 → OpenRB (test GPIO)
static const uint8_t PIN_LED = D4;      // GPIO2 = LED intégrée

// UART: RX=GPIO13(D7), TX=GPIO15(D8) pour communiquer avec OpenRB
static const uint8_t RX_PIN = D7;       // GPIO13
static const uint8_t TX_PIN = D8;       // GPIO15
SoftwareSerial openrbSerial(RX_PIN, TX_PIN);

// ===== Serveur Web =====
ESP8266WebServer server(HTTP_PORT);

// ===== État du système =====
struct SystemState {
    bool pinState = false;
    bool openrbConnected = false;
    uint32_t uptime = 0;
    int rssi = 0;
    const char* version = "1.0.0";
} state;

// ===== Fonction auxiliaire: Vérifier le token Bearer =====
bool verifyAuthToken() {
    if (!server.hasHeader("Authorization")) {
        return false;
    }
    String authHeader = server.header("Authorization");
    String expectedAuth = String("Bearer ") + String(AUTH_TOKEN);
    return authHeader == expectedAuth;
}

// ===== Endpoint: GET /api/status =====
void handleStatus() {
    if (!verifyAuthToken()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }

    StaticJsonDocument<256> doc;
    doc["status"] = "ok";
    doc["pin_state"] = state.pinState ? "HIGH" : "LOW";
    doc["openrb_connected"] = state.openrbConnected;
    doc["uptime_ms"] = millis();
    doc["rssi"] = WiFi.RSSI();
    doc["version"] = state.version;
    doc["ip"] = WiFi.localIP().toString();

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
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
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    if (!doc.containsKey("command")) {
        server.send(400, "application/json", "{\"error\":\"Missing command field\"}");
        return;
    }

    String command = doc["command"].as<String>();
    command.toUpperCase();

    // Exécuter la commande
    if (command == "HIGH") {
        digitalWrite(PIN_OUT, HIGH);
        digitalWrite(PIN_LED, LOW);   // LED actif bas = allumée
        state.pinState = true;
        openrbSerial.write('H');
        openrbSerial.write('\n');
        Serial.println("[REST] Command: HIGH");
    }
    else if (command == "LOW") {
        digitalWrite(PIN_OUT, LOW);
        digitalWrite(PIN_LED, HIGH);  // LED actif bas = éteinte
        state.pinState = false;
        openrbSerial.write('L');
        openrbSerial.write('\n');
        Serial.println("[REST] Command: LOW");
    }
    else if (command == "STATUS") {
        // Just respond with current state
        Serial.println("[REST] Command: STATUS");
    }
    else {
        server.send(400, "application/json", "{\"error\":\"Unknown command\"}");
        return;
    }

    // Répondre avec succès
    StaticJsonDocument<256> response;
    response["result"] = "success";
    response["command"] = command;
    response["pin_state"] = state.pinState ? "HIGH" : "LOW";
    response["timestamp"] = millis();

    String responseStr;
    serializeJson(response, responseStr);
    server.send(200, "application/json", responseStr);
}

// ===== Gestion des requêtes non trouvées =====
void handleNotFound() {
    server.send(404, "application/json",
        "{\"error\":\"Not found\",\"path\":\"" + server.uri() + "\"}");
}

// ===== Lecture de la réponse OpenRB =====
void readOpenRBResponse() {
    while (openrbSerial.available()) {
        String response = openrbSerial.readStringUntil('\n');
        Serial.print("[OpenRB] ");
        Serial.println(response);
    }
}

// ===== Initialisation WiFi =====
bool setupWiFi() {
    Serial.print("\n[WiFi] Connecting to ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Failed to connect");
        return false;
    }

    Serial.print("[WiFi] Connected! IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WiFi] RSSI: ");
    Serial.println(WiFi.RSSI());

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
    Serial.println("\n\n=== NodeMCU ESP8266 — WiFi REST Gateway ===");

    // Initialiser pins
    pinMode(PIN_OUT, OUTPUT);
    digitalWrite(PIN_OUT, LOW);
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);   // LED off

    // Initialiser UART vers OpenRB
    openrbSerial.begin(115200);
    Serial.println("[Serial] OpenRB serial initialized at 115200 baud");

    // WiFi
    if (!setupWiFi()) {
        Serial.println("[FATAL] WiFi connection failed!");
        while (true) {
            digitalWrite(PIN_LED, !digitalRead(PIN_LED));
            delay(200);
        }
    }

    // Serveur HTTP
    setupServer();

    Serial.println("[Boot] Ready to receive commands via REST API");
    Serial.print("[Boot] Send requests to: http://");
    Serial.print(WiFi.localIP());
    Serial.print(":");
    Serial.println(HTTP_PORT);
}

// ===== Loop principal =====
void loop() {
    // Traiter les requêtes HTTP
    server.handleClient();

    // Lire les réponses de l'OpenRB
    readOpenRBResponse();

    // Blink d'activité lent
    static uint32_t lastBlink = 0;
    if (millis() - lastBlink > 1000) {
        digitalWrite(PIN_LED, !digitalRead(PIN_LED));
        lastBlink = millis();
    }
}
