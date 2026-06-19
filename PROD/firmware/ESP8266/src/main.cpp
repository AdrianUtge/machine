/**
 * NodeMCU v3 (ESP8266) — Binary Protocol WebSocket Gateway (Phase 3)
 *
 * Replaces HTTP REST server with WebSocket server for real-time binary frame streaming.
 * Acts as a transparent serial bridge: WiFi ↔ UART1/SoftwareSerial ↔ OpenRB-150.
 *
 * Protocol: 17_BINARY_PROTOCOL.md
 * - Command frames (0xC): host → device, 3–8 bytes
 * - Response frames (0xR): device → host, 3 bytes
 * - Status frames (0xS): device → host (continuous), 20 bytes
 *
 * Endpoints:
 *  WS  /ws          → WebSocket (binary frame streaming)
 *  GET /api/status  → JSON (cached STATUS frame, for fallback polling)
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include "config.h"

// ===== Hardware =====
static const uint8_t PIN_LED = D4;  // GPIO2 = LED intégrée (actif bas)

// ===== Serial Link to OpenRB-150 =====
// Try UART1 first (RX=GPIO13/D7, TX=GPIO15/D8 @ 19200)
// UART1 is hardware serial, much faster than SoftwareSerial @ 9600
// Fallback: SoftwareSerial @ 19200 (still 2× faster than old 9600)
//
// NOTE: On NodeMCU, UART1 RX pin is special (GPIO13) and TX is GPIO15.
// We use Serial1 (UART1) if available, otherwise fall back to SoftwareSerial.
//
#define OPENRB_BAUD 19200  // Upgraded from 9600 (UART1 can handle this)

// Try to use hardware UART1 (Serial1) for speed; fall back to SoftwareSerial
// UART1 TX = GPIO15 (D8), RX = GPIO13 (D7)
static HardwareSerial* openrb_link = &Serial1;  // UART1 (fast, hardware)

// Fallback: SoftwareSerial if UART1 has issues
// SoftwareSerial openrb_fallback(14 /*RX=GPIO14*/, 12 /*TX=GPIO12*/);

// ===== WebSocket Server =====
ESP8266WebServer server(HTTP_PORT);
WebSocketsServer webSocket = WebSocketsServer(8080);

// ===== Binary Frame Parsing Buffer =====
static const size_t RX_BUFFER_SIZE = 128;
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static size_t rx_pos = 0;
static uint32_t rx_last_byte_ms = 0;

// ===== Cached Status Frame (last 0xS received from OpenRB) =====
static uint8_t cached_status_frame[20] = {0};
static bool has_cached_status = false;
static uint32_t last_status_ms = 0;

// ===== System State (fallback for /api/status without WebSocket) =====
struct SystemState {
    uint32_t uptime_ms = 0;
    int rssi = 0;
    const char* version = "3.0.0-phase3-websocket";
} state;

// ===== CRC8 Checksum (matches OpenRB firmware and protocol spec) =====
static uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = ((crc << 1) ^ 0x07) & 0xFF;
            } else {
                crc = (crc << 1) & 0xFF;
            }
        }
    }
    return crc;
}

static bool validate_frame_checksum(const uint8_t* frame, size_t len) {
    if (len < 2) return false;
    uint8_t expected_crc = frame[len - 1];
    uint8_t computed_crc = crc8(frame, len - 1);
    return computed_crc == expected_crc;
}

// ===== Frame Detection & Extraction =====
static bool is_complete_frame(const uint8_t* buf, size_t len, size_t& frame_size) {
    if (len < 2) return false;

    uint8_t frame_type = buf[0];

    if (frame_type == 0x43) {  // 'C' = COMMAND
        // Variable length (3–8 bytes), detect by CRC8
        for (size_t try_len = 3; try_len <= min(len, (size_t)8); try_len++) {
            if (validate_frame_checksum(buf, try_len)) {
                frame_size = try_len;
                return true;
            }
        }
        return false;

    } else if (frame_type == 0x52) {  // 'R' = RESPONSE
        // Fixed 3 bytes: [0xR][CODE][CRC8]
        if (len >= 3 && validate_frame_checksum(buf, 3)) {
            frame_size = 3;
            return true;
        }
        return false;

    } else if (frame_type == 0x53) {  // 'S' = STATUS
        // Fixed 20 bytes: [0xS][FREQ:2][POS[4]:8][FORCE[4]:8][CRC8]
        if (len >= 20 && validate_frame_checksum(buf, 20)) {
            frame_size = 20;
            return true;
        }
        return false;
    }

    return false;
}

// ===== UART1 Pump: Read from OpenRB, forward to WebSocket, cache STATUS =====
static void pumpOpenRB() {
    // Read available bytes from UART1
    while (openrb_link->available()) {
        uint8_t byte = openrb_link->read();
        rx_last_byte_ms = millis();

        // Add to buffer
        if (rx_pos < RX_BUFFER_SIZE) {
            rx_buffer[rx_pos++] = byte;
        } else {
            // Buffer overflow: discard oldest half and shift
            memmove(rx_buffer, rx_buffer + RX_BUFFER_SIZE / 2, RX_BUFFER_SIZE / 2);
            rx_pos = RX_BUFFER_SIZE / 2;
            rx_buffer[rx_pos++] = byte;
        }

        // Try to detect a complete frame
        size_t frame_size = 0;
        if (is_complete_frame(rx_buffer, rx_pos, frame_size)) {
            // Found a complete frame
            uint8_t* frame = rx_buffer;

            // Cache STATUS frames (0xS) for GET /api/status fallback
            if (frame[0] == 0x53 && frame_size == 20) {
                memcpy(cached_status_frame, frame, 20);
                has_cached_status = true;
                last_status_ms = millis();
            }

            // Broadcast frame to all WebSocket clients
            webSocket.broadcastBIN(frame, frame_size);

            // Shift buffer: remove this frame, keep remainder
            memmove(rx_buffer, frame + frame_size, rx_pos - frame_size);
            rx_pos -= frame_size;
        }
    }

    // Timeout: if no bytes for 200ms, discard incomplete frame
    if (rx_pos > 0 && (millis() - rx_last_byte_ms > 200)) {
        rx_pos = 0;
    }
}

// ===== WebSocket Event Handler =====
static void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            Serial.printf("[WS] Client %u connected\n", num);
            // Send cached STATUS frame immediately to new client
            if (has_cached_status) {
                webSocket.sendBIN(num, cached_status_frame, 20);
            }
            break;

        case WStype_DISCONNECTED:
            Serial.printf("[WS] Client %u disconnected\n", num);
            break;

        case WStype_BIN:
            // Forward binary frame to OpenRB via UART1
            if (length > 0 && length <= 8) {
                openrb_link->write(payload, length);
                openrb_link->flush();
                Serial.printf("[WS>UART] Forwarded %u bytes\n", length);
            }
            break;

        case WStype_TEXT:
        case WStype_ERROR:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
        default:
            break;
    }
}

// ===== Endpoint: GET /api/status (JSON fallback for polling) =====
static void handleStatus() {
    // CORS headers
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");

    // Build JSON response from cached STATUS frame (if available)
    JsonDocument doc;
    doc["status"] = "ok";
    doc["uptime_ms"] = millis();
    doc["rssi"] = WiFi.RSSI();
    doc["version"] = state.version;
    doc["ip"] = WiFi.localIP().toString();
    doc["websocket_active"] = webSocket.connectedClients() > 0;

    if (has_cached_status) {
        // Decode cached STATUS frame: [0xS][FREQ:2LE][POS[4]:2LE][FORCE[4]:2LE][CRC8]
        uint16_t freq_hz10 = cached_status_frame[1] | (cached_status_frame[2] << 8);
        float freq_hz = freq_hz10 / 10.0;
        doc["frequency"] = freq_hz;

        JsonArray pos_arr = doc["positions"].to<JsonArray>();
        for (int i = 0; i < 4; i++) {
            uint16_t pos_mm10 = cached_status_frame[3 + i*2] |
                                (cached_status_frame[4 + i*2] << 8);
            pos_arr.add(pos_mm10 / 10.0);
        }

        JsonArray force_arr = doc["forces"].to<JsonArray>();
        for (int i = 0; i < 4; i++) {
            uint16_t force_mv = cached_status_frame[11 + i*2] |
                                (cached_status_frame[12 + i*2] << 8);
            force_arr.add(force_mv / 1000.0);  // Convert mV → N (assuming 1mV ≈ 1mN)
        }

        doc["status_fresh_ms"] = millis() - last_status_ms;
    } else {
        doc["status"] = "no_openrb_data";
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// ===== Endpoint: OPTIONS (CORS preflight) =====
static void handleOptions() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
    server.send(204);  // No Content
}

// ===== LED Heartbeat =====
static void updateLED() {
    static uint32_t last_toggle = 0;
    if (millis() - last_toggle >= 500) {
        digitalWrite(PIN_LED, !digitalRead(PIN_LED));  // Toggle @ 1 Hz
        last_toggle = millis();
    }
}

// ===== Setup =====
void setup() {
    Serial.begin(115200);
    delay(100);

    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);  // LED off (active low)

    Serial.println("\n[SETUP] Starting WiFi...");

    // WiFi AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("[SETUP] AP started: %s\n", WIFI_SSID);
    Serial.printf("[SETUP] IP: %s\n", WiFi.softAPIP().toString().c_str());

    // Initialize UART1 for OpenRB link (pins RX=GPIO13, TX=GPIO15 are fixed on ESP8266)
    Serial.println("[SETUP] Initializing UART1 @ 19200 baud...");
    Serial1.begin(OPENRB_BAUD);  // UART1 on ESP8266 uses fixed pins RX=GPIO13, TX=GPIO15
    delay(100);
    Serial.println("[SETUP] UART1 ready");

    // WebSocket server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println("[SETUP] WebSocket server started on :8080/ws");

    // HTTP server
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/status", HTTP_OPTIONS, handleOptions);

    // Handle other OPTIONS requests (CORS preflight)
    server.onNotFound([]() {
        if (server.method() == HTTP_OPTIONS) {
            handleOptions();
        } else {
            server.send(404, "text/plain", "Not Found");
        }
    });

    server.begin();
    Serial.println("[SETUP] HTTP server started on :80");
    Serial.println("[SETUP] Ready!");
}

// ===== Main Loop =====
void loop() {
    // Handle HTTP requests
    server.handleClient();

    // Handle WebSocket events
    webSocket.loop();

    // Pump data from OpenRB (UART1 → WebSocket)
    pumpOpenRB();

    // Update LED heartbeat
    updateLED();
}
