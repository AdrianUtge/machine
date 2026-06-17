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
#include <SoftwareSerial.h>
#include "config.h"

// ===== Hardware =====
static const uint8_t PIN_LED = D4;      // GPIO2 = LED intégrée (actif bas)

// ===== Lien série vers l'OpenRB-150 =====
// ESP TX = GPIO12 (D6) -> OpenRB Serial3 RX (D13)
// ESP RX = GPIO14 (D5) <- OpenRB Serial3 TX (D14)
#define OPENRB_BAUD 19200               // baud modéré = SoftwareSerial fiable malgré le WiFi
SoftwareSerial openrb(14 /*RX=GPIO14*/, 12 /*TX=GPIO12*/);

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

// ===== Cache "live" de l'OpenRB (liaison permanente) =====
// L'OpenRB streame son statut en burst ~10 Hz. On garde le dernier ici pour
// répondre à GET /api/status instantanément, sans aller-retour série.
struct OpenRbLive {
    String   rbState   = "UNKNOWN";
    float    frequency = 0.0f;          // Hz réels rapportés par l'OpenRB
    float    positions[4] = {0, 0, 0, 0};
    float    sensors[4]   = {0, 0, 0, 0};   // forces lues (N)
    bool     slaveOnline  = false;
    uint32_t lastDataMs   = 0;          // millis() de la dernière ligne reçue
} live;

// Découpe "a,b,c,d" -> out[4] (tolère < 4 valeurs).
static void parseCsv4(const String& csv, float out[4]) {
    int idx = 0, start = 0;
    while (idx < 4) {
        int comma = csv.indexOf(',', start);
        String tok = (comma < 0) ? csv.substring(start) : csv.substring(start, comma);
        out[idx++] = tok.toFloat();
        if (comma < 0) break;
        start = comma + 1;
    }
}

// Met à jour le cache à partir d'une ligne protocole reçue de l'OpenRB.
static void parseOpenRbLine(const String& line) {
    int sep = line.indexOf(':');
    if (sep < 0) return;
    String key = line.substring(0, sep); key.toUpperCase();
    String val = line.substring(sep + 1); val.trim();
    live.lastDataMs = millis();
    if      (key == "STATE")    live.rbState = val;
    else if (key == "FREQ")     live.frequency = val.toFloat();
    else if (key == "POSITION") parseCsv4(val, live.positions);
    else if (key == "FORCE")    parseCsv4(val, live.sensors);
    else if (key == "SLAVE")    live.slaveOnline = (val == "ONLINE");
    // ACK:/ERR: -> ignorés pour le cache (mais comptent comme "data reçue")
}

// Lecture continue du flux OpenRB (appelée à chaque loop()). UN seul lecteur
// du port série : sendToOpenRB() n'écrit que, ne lit jamais.
static void pumpOpenRB() {
    static String buf;
    while (openrb.available()) {
        char c = (char)openrb.read();
        if (c == '\n' || c == '\r') {
            if (buf.length()) { parseOpenRbLine(buf); buf = ""; }
        } else if (buf.length() < 120) {
            buf += c;
        }
    }
}

// OpenRB considéré vivant si une ligne est arrivée il y a moins d'1 s.
static bool openRbFresh() {
    return live.lastDataMs != 0 && (millis() - live.lastDataMs) < 1000;
}

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

    StaticJsonDocument<640> doc;
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

    // --- Données LIVE de l'OpenRB (cache alimenté par le streaming) ---
    doc["rb_state"] = live.rbState;
    doc["rb_frequency"] = live.frequency;
    doc["rb_online"] = openRbFresh() && live.slaveOnline;
    doc["rb_fresh_ms"] = live.lastDataMs ? (int32_t)(millis() - live.lastDataMs) : -1;
    JsonArray posArr = doc.createNestedArray("positions");
    for (int i = 0; i < 4; i++) posArr.add(live.positions[i]);
    JsonArray senArr = doc.createNestedArray("sensors");
    for (int i = 0; i < 4; i++) senArr.add(live.sensors[i]);

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);

    Serial.println("[REST] Status sent");
}

// ===== OpenRB-150 : mapping commande JSON -> protocole ligne =====
String buildOpenRbLine(JsonDocument& doc, const String& cmd) {
    if (cmd == "FREQUENCY") return "SET_FREQ:" + String(doc["frequency"].as<float>(), 3);
    if (cmd == "SPEED")     return "SET_SPEED:" + String(doc["speed"].as<int>());
    if (cmd == "FORCE") {
        float f = doc["force"].as<float>();
        if (doc.containsKey("sensor"))
            return "SET_FORCE:" + String(doc["sensor"].as<int>()) + ":" + String(f, 3);
        return "SET_FORCE:" + String(f, 3);
    }
    if (cmd == "GOTO")
        return "GOTO:" + String(doc["table"].as<int>()) + ":" + String(doc["position"].as<float>(), 3);
    if (cmd == "STATUS")    return "GET_STATUS";
    // START / STOP / HOME / HARD_RESET (et fallback) : commande telle quelle
    return cmd;
}

// ===== OpenRB-150 : envoyer une ligne (fire-and-forget) =====
// La lecture des réponses est faite en continu par pumpOpenRB() : on n'attend
// donc plus la réponse ici -> fini la fenêtre de silence de 80 ms par commande.
void sendToOpenRB(const String& line) {
    openrb.print(line);
    openrb.print('\n');
    Serial.print("[OpenRB] > ");
    Serial.println(line);
}

// Snapshot du cache live sous forme de lignes protocole (réponse immédiate au POST).
void appendLiveLines(JsonArray& outLines) {
    outLines.add(String("STATE:") + live.rbState);
    outLines.add(String("FREQ:") + String(live.frequency, 3));
    String pos = "POSITION:";
    for (int i = 0; i < 4; i++) { pos += String(live.positions[i], 2); if (i < 3) pos += ','; }
    outLines.add(pos);
    String frc = "FORCE:";
    for (int i = 0; i < 4; i++) { frc += String(live.sensors[i], 3); if (i < 3) frc += ','; }
    outLines.add(frc);
    outLines.add(String("SLAVE:") + (live.slaveOnline ? "ONLINE" : "OFFLINE"));
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
        "FREQUENCY", "SPEED", "FORCE", "GOTO", "PRESET", "MANUAL", "STATUS",
        "TORQUE_ON", "TORQUE_OFF"
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

    // Phase 2 : transmettre à l'OpenRB-150 et collecter ses réponses
    StaticJsonDocument<768> response;
    response["result"] = "success";
    response["command"] = command;
    response["command_number"] = state.commandCount;

    String line = buildOpenRbLine(doc, command);
    sendToOpenRB(line);
    // Réponse immédiate : snapshot du cache live (pas d'attente série). Le statut
    // réel suit dans <100 ms via le streaming + GET /api/status (déjà mis en cache).
    JsonArray lines = response.createNestedArray("lines");
    appendLiveLines(lines);

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

    // Lien série vers l'OpenRB-150
    openrb.begin(OPENRB_BAUD);
    Serial.print("[OpenRB] SoftwareSerial @ ");
    Serial.print(OPENRB_BAUD);
    Serial.println(" baud (RX=GPIO14, TX=GPIO12)");

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
    // Lecture continue du flux OpenRB (liaison permanente) -> alimente le cache.
    pumpOpenRB();

    // Traiter les requêtes HTTP
    server.handleClient();

    // LED status indicator
    static uint32_t lastBlink = 0;
    if (millis() - lastBlink > 1000) {
        digitalWrite(PIN_LED, !digitalRead(PIN_LED));  // Toggle LED
        lastBlink = millis();
    }
}
