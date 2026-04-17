#include "command_dispatcher.h"

// --- Constructor -----------------------------------------------------------

CommandDispatcher::CommandDispatcher(MachineState& machineState)
    : _machineState(machineState)
{
}

// --- Public API ------------------------------------------------------------

void CommandDispatcher::dispatch(const String& line, Stream& out)
{
    const String trimmedLine = line;
    const int separatorIndex = trimmedLine.indexOf(':');

    String command;
    String argument;

    if (separatorIndex >= 0)
    {
        command = trimmedLine.substring(0, separatorIndex);
        argument = trimmedLine.substring(separatorIndex + 1);
    }
    else
    {
        command = trimmedLine;
        argument = "";
    }

    command.trim();
    argument.trim();
    command.toUpperCase();

    if (command.length() == 0)
    {
        sendError("EMPTY_COMMAND", out);
        return;
    }

    if (command == "HOME")
    {
        if (argument.length() > 0)
        {
            sendError("HOME_TAKES_NO_ARGUMENT", out);
            return;
        }

        handleHome(out);
        return;
    }

    if (command == "START")
    {
        if (argument.length() > 0)
        {
            sendError("START_TAKES_NO_ARGUMENT", out);
            return;
        }

        handleStart(out);
        return;
    }

    if (command == "HARD_RESET")
    {
        if (argument.length() > 0)
        {
            sendError("HARD_RESET_TAKES_NO_ARGUMENT", out);
            return;
        }

        handleHardReset(out);
        return;
    }

    if (command == "SET_SPEED")
    {
        if (argument.length() == 0)
        {
            sendError("MISSING_SPEED_VALUE", out);
            return;
        }

        handleSetSpeed(argument, out);
        return;
    }

    if (command == "SET_FREQ")
    {
        if (argument.length() == 0)
        {
            sendError("MISSING_FREQ_VALUE", out);
            return;
        }

        handleSetFreq(argument, out);
        return;
    }

    if (command == "GET_STATUS")
    {
        if (argument.length() > 0)
        {
            sendError("GET_STATUS_TAKES_NO_ARGUMENT", out);
            return;
        }

        handleGetStatus(out);
        return;
    }

    sendError("UNKNOWN_COMMAND", out);
}

// --- Handlers --------------------------------------------------------------

void CommandDispatcher::handleHome(Stream& out)
{
    _machineState.home();
    sendAck("HOME", out);
}

void CommandDispatcher::handleStart(Stream& out)
{
    if (!_machineState.isHomed())
    {
        sendError("NOT_HOMED", out);
        return;
    }

    _machineState.start();
    sendAck("START", out);
}

void CommandDispatcher::handleHardReset(Stream& out)
{
    _machineState.hardReset();
    sendAck("HARD_RESET", out);
}

void CommandDispatcher::handleSetSpeed(const String& value, Stream& out)
{
    int32_t speed = 0;

    if (!parseInt32(value, speed))
    {
        sendError("INVALID_SPEED_FORMAT", out);
        return;
    }

    if (!_machineState.setSpeed(speed))
    {
        sendError("SPEED_OUT_OF_RANGE", out);
        return;
    }

    sendAck("SET_SPEED", out);
}

void CommandDispatcher::handleSetFreq(const String& value, Stream& out)
{
    float frequency = 0.0f;

    if (!parseFloat32(value, frequency))
    {
        sendError("INVALID_FREQ_FORMAT", out);
        return;
    }

    if (!_machineState.setFrequency(frequency))
    {
        sendError("FREQ_OUT_OF_RANGE", out);
        return;
    }

    sendAck("SET_FREQ", out);
}

void CommandDispatcher::handleGetStatus(Stream& out)
{
    out.print("STATE:");
    out.println(_machineState.stateToString());

    out.print("FREQ:");
    out.println(_machineState.getFrequency(), 3);

    out.print("POSITION:");
    out.println(_machineState.getPosition(), 3);

    out.print("CURRENT:");
    out.println(_machineState.getCurrent(), 3);

    out.print("FORCE:");
    out.println(_machineState.getForce(), 3);

    out.print("SLAVE:");
    out.println(_machineState.getSlaveStatus());

    sendAck("GET_STATUS", out);
}

// --- Parsing helpers -------------------------------------------------------

bool CommandDispatcher::parseInt32(const String& text, int32_t& value) const
{
    if (text.length() == 0)
    {
        return false;
    }

    char* endPtr = nullptr;
    const long parsedValue = strtol(text.c_str(), &endPtr, 10);

    if ((endPtr == text.c_str()) || (*endPtr != '\0'))
    {
        return false;
    }

    value = static_cast<int32_t>(parsedValue);
    return true;
}

bool CommandDispatcher::parseFloat32(const String& text, float& value) const
{
    if (text.length() == 0)
    {
        return false;
    }

    char* endPtr = nullptr;
    const float parsedValue = strtof(text.c_str(), &endPtr);

    if ((endPtr == text.c_str()) || (*endPtr != '\0'))
    {
        return false;
    }

    value = parsedValue;
    return true;
}

// --- Output helpers --------------------------------------------------------

void CommandDispatcher::sendAck(const String& message, Stream& out) const
{
    out.print("ACK:");
    out.println(message);
}

void CommandDispatcher::sendError(const String& message, Stream& out) const
{
    out.print("ERROR:");
    out.println(message);
}