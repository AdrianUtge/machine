#include <Arduino.h>
#include <Dynamixel2Arduino.h>

// --- Serial ports ----------------------------------------------------------

#define DEBUG_SERIAL Serial     // USB vers le PC
#define LINK_SERIAL  Serial1    // UART vers le master

// --- Dynamixel -------------------------------------------------------------

const float DXL_PROTOCOL_VERSION = 2.0f;
constexpr uint8_t DXL_ID = 1;
constexpr int DXL_DIR_PIN = -1;   // adapte si besoin sur OpenRB-150

Dynamixel2Arduino dxl(LINK_SERIAL, DXL_DIR_PIN);

// --- Constants -------------------------------------------------------------

constexpr uint8_t VELOCITY_MODE = 1;
constexpr float GOAL_VELOCITY_UNIT_RPM = 0.229f;  // XL330
constexpr unsigned long STATUS_PERIOD_MS = 250;

// --- State ----------------------------------------------------------------

float g_frequency_hz = 0.8f;
bool g_running = false;
unsigned long g_lastStatusMs = 0;

// --- Helpers ---------------------------------------------------------------

int32_t frequencyToGoalVelocity(float freq_hz)
{
    if (freq_hz < 0.0f) {
        freq_hz = 0.0f;
    }

    const float rpm = freq_hz * 60.0f;
    return static_cast<int32_t>(lroundf(rpm / GOAL_VELOCITY_UNIT_RPM));
}

void logBoth(const String& line)
{
    DEBUG_SERIAL.println(line);
    LINK_SERIAL.println(line);
}

void logUsb(const String& line)
{
    DEBUG_SERIAL.println(line);
}

void sendToMaster(const String& line)
{
    LINK_SERIAL.println(line);
}

void applyVelocityCommand()
{
    const int32_t goal = g_running ? frequencyToGoalVelocity(g_frequency_hz) : 0;
    dxl.writeControlTableItem(ControlTableItem::GOAL_VELOCITY, DXL_ID, goal);

    logUsb("DBG:GOAL_VEL_RAW:" + String(goal));
}

void startMotor()
{
    g_running = true;
    applyVelocityCommand();
    logBoth("ACK:START");
}

void stopMotor()
{
    g_running = false;
    applyVelocityCommand();
    logBoth("ACK:STOP");
}

void setFrequency(float f)
{
    g_frequency_hz = f;
    applyVelocityCommand();

    sendToMaster("ACK:SET_FREQ:" + String(g_frequency_hz, 3));
    logUsb("ACK:SET_FREQ:" + String(g_frequency_hz, 3));
}

void sendStatus()
{
    const int32_t present_velocity = dxl.readControlTableItem(ControlTableItem::PRESENT_VELOCITY, DXL_ID);
    const int32_t present_position = dxl.readControlTableItem(ControlTableItem::PRESENT_POSITION, DXL_ID);
    const int16_t present_current  = dxl.readControlTableItem(ControlTableItem::PRESENT_CURRENT, DXL_ID);

    sendToMaster("STATE:" + String(g_running ? "RUNNING" : "IDLE"));
    sendToMaster("FREQ:" + String(g_frequency_hz, 3));
    sendToMaster("POS:" + String(present_position));
    sendToMaster("CUR_mA:" + String(present_current));
    sendToMaster("VEL_RAW:" + String(present_velocity));

    logUsb("STATE:" + String(g_running ? "RUNNING" : "IDLE"));
    logUsb("FREQ:" + String(g_frequency_hz, 3));
    logUsb("POS:" + String(present_position));
    logUsb("CUR_mA:" + String(present_current));
    logUsb("VEL_RAW:" + String(present_velocity));
}

bool configureMotor()
{
    LINK_SERIAL.begin(57600);
    dxl.setPortProtocolVersion(DXL_PROTOCOL_VERSION);

    if (!dxl.ping(DXL_ID)) {
        return false;
    }

    dxl.torqueOff(DXL_ID);
    dxl.writeControlTableItem(ControlTableItem::OPERATING_MODE, DXL_ID, VELOCITY_MODE);
    dxl.torqueOn(DXL_ID);

    stopMotor();
    return true;
}

String readLineFrom(Stream& serial, String& buffer)
{
    while (serial.available() > 0) {
        const char c = static_cast<char>(serial.read());

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            String line = buffer;
            buffer = "";
            line.trim();
            return line;
        }

        buffer += c;
    }

    return "";
}

void handleCommand(const String& raw, bool fromUsb)
{
    String cmd = raw;
    cmd.trim();

    if (cmd.length() == 0) {
        return;
    }

    if (fromUsb) {
        logUsb("USB_CMD:" + cmd);
    } else {
        logUsb("UART_CMD:" + cmd);
    }

    const int sep = cmd.indexOf(':');
    String name = (sep >= 0) ? cmd.substring(0, sep) : cmd;
    String arg  = (sep >= 0) ? cmd.substring(sep + 1) : "";

    name.trim();
    arg.trim();
    name.toUpperCase();

    if (name == "START") {
        startMotor();
        return;
    }

    if (name == "STOP") {
        stopMotor();
        return;
    }

    if (name == "SET_FREQ") {
        const float f = arg.toFloat();

        if (f < 0.0f) {
            sendToMaster("ERROR:FREQ_OUT_OF_RANGE");
            logUsb("ERROR:FREQ_OUT_OF_RANGE");
            return;
        }

        setFrequency(f);
        return;
    }

    if (name == "GET_STATUS") {
        sendStatus();
        sendToMaster("ACK:GET_STATUS");
        logUsb("ACK:GET_STATUS");
        return;
    }

    sendToMaster("ERROR:UNKNOWN_COMMAND");
    logUsb("ERROR:UNKNOWN_COMMAND");
}

// --- Arduino ---------------------------------------------------------------

void setup()
{
    DEBUG_SERIAL.begin(115200);

    while (!DEBUG_SERIAL) {
        delay(10);
    }

    DEBUG_SERIAL.println("DBG:BOOTING_SLAVE");

    const bool ok = configureMotor();

    if (ok) {
        DEBUG_SERIAL.println("ACK:BOOT");
        DEBUG_SERIAL.println("STATE:IDLE");

        sendToMaster("ACK:BOOT");
        sendToMaster("STATE:IDLE");
    } else {
        DEBUG_SERIAL.println("ERROR:DXL_NOT_FOUND");
        sendToMaster("ERROR:DXL_NOT_FOUND");
    }
}

void loop()
{
    static String usbBuffer = "";
    static String uartBuffer = "";

    const String uartLine = readLineFrom(LINK_SERIAL, uartBuffer);
    if (uartLine.length() > 0) {
        handleCommand(uartLine, false);
    }

    const String usbLine = readLineFrom(DEBUG_SERIAL, usbBuffer);
    if (usbLine.length() > 0) {
        handleCommand(usbLine, true);
    }

    const unsigned long now = millis();
    if ((now - g_lastStatusMs) >= STATUS_PERIOD_MS) {
        g_lastStatusMs = now;
        sendStatus();
    }
}