#include <Arduino.h>
#include "serial_parser.h"
#include "command_dispatcher.h"
#include "machine_state.h"

// --- Globals ---------------------------------------------------------------

static MachineState g_machineState;
static SerialParser g_serialParser;
static CommandDispatcher g_commandDispatcher(g_machineState);

// --- Arduino setup ---------------------------------------------------------

void setup()
{
    Serial.begin(115200);

    while (!Serial)
    {
        delay(10);
    }

    g_machineState.begin();
    g_serialParser.begin();

    Serial.println("ACK:BOOT");
    Serial.println("STATE:IDLE");
}

// --- Arduino loop ----------------------------------------------------------

void loop()
{
    g_serialParser.update(Serial);

    int iterations = 0;
    while (g_serialParser.hasLine() && iterations < 10)
    {
        String line = g_serialParser.popLine();
        g_commandDispatcher.dispatch(line, Serial);
        iterations++;
    }

    g_machineState.update();
}