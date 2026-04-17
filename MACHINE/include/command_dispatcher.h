#pragma once

#include <Arduino.h>

#include "machine_state.h"

// --- CommandDispatcher -----------------------------------------------------

class CommandDispatcher
{
public:
    explicit CommandDispatcher(MachineState& machineState);

    void dispatch(const String& line, Stream& out);

private:
    void handleHome(Stream& out);
    void handleStart(Stream& out);
    void handleHardReset(Stream& out);
    void handleSetSpeed(const String& value, Stream& out);
    void handleSetFreq(const String& value, Stream& out);
    void handleGetStatus(Stream& out);

    bool parseInt32(const String& text, int32_t& value) const;
    bool parseFloat32(const String& text, float& value) const;

    void sendAck(const String& message, Stream& out) const;
    void sendError(const String& message, Stream& out) const;

private:
    MachineState& _machineState;
};