#include "slave_link.h"

#include <stdlib.h>
#include <string.h>

// --- Constants -------------------------------------------------------------

static constexpr unsigned long SLAVE_OFFLINE_TIMEOUT_MS = 1000U;
static constexpr unsigned long SLAVE_STATUS_POLL_MS = 250U;

// --- Constructor -----------------------------------------------------------

SlaveLink::SlaveLink()
    : _serial(nullptr),
      _online(false),
      _rxIndex(0U),
      _frequency(0.0f),
      _position(0.0f),
      _current(0.0f),
      _lastRxMs(0U),
      _lastPollMs(0U)
{
    resetRxBuffer();
    strncpy(_state, "UNKNOWN", sizeof(_state) - 1U);
    _state[sizeof(_state) - 1U] = '\0';
}

// --- Public API ------------------------------------------------------------

void SlaveLink::begin(HardwareSerial& serial, uint32_t baudrate)
{
    _serial = &serial;
    _serial->begin(baudrate);

    _online = false;
    _rxIndex = 0U;
    _frequency = 0.0f;
    _position = 0.0f;
    _current = 0.0f;
    _lastRxMs = millis();
    _lastPollMs = millis();

    strncpy(_state, "UNKNOWN", sizeof(_state) - 1U);
    _state[sizeof(_state) - 1U] = '\0';

    resetRxBuffer();
}

void SlaveLink::update()
{
    if (_serial == nullptr)
    {
        return;
    }

    while (_serial->available() > 0)
    {
        const char c = static_cast<char>(_serial->read());

        if (c == '\r')
        {
            continue;
        }

        if (c == '\n')
        {
            _rxBuffer[_rxIndex] = '\0';

            if (_rxIndex > 0U)
            {
                handleLine(String(_rxBuffer));
            }

            resetRxBuffer();
            continue;
        }

        if (_rxIndex < (sizeof(_rxBuffer) - 1U))
        {
            _rxBuffer[_rxIndex++] = c;
            _rxBuffer[_rxIndex] = '\0';
        }
        else
        {
            resetRxBuffer();
        }
    }

    const unsigned long now = millis();

    if ((now - _lastPollMs) >= SLAVE_STATUS_POLL_MS)
    {
        _lastPollMs = now;
        sendGetStatus();
    }

    _online = ((now - _lastRxMs) <= SLAVE_OFFLINE_TIMEOUT_MS);
}

void SlaveLink::sendStart()
{
    sendLine("START");
}

void SlaveLink::sendStop()
{
    sendLine("STOP");
}

void SlaveLink::sendSetFrequency(float frequencyHz)
{
    String line = "SET_FREQ:";
    line += String(frequencyHz, 3);
    sendLine(line);
}

void SlaveLink::sendGetStatus()
{
    sendLine("GET_STATUS");
}

bool SlaveLink::isOnline() const
{
    return _online;
}

const char* SlaveLink::getState() const
{
    return _state;
}

float SlaveLink::getFrequency() const
{
    return _frequency;
}

float SlaveLink::getPosition() const
{
    return _position;
}

float SlaveLink::getCurrent() const
{
    return _current;
}

// --- Private helpers -------------------------------------------------------

void SlaveLink::sendLine(const String& line)
{
    if (_serial == nullptr)
    {
        return;
    }

    _serial->println(line);
}

void SlaveLink::handleLine(const String& line)
{
    _lastRxMs = millis();
    _online = true;

    const int separatorIndex = line.indexOf(':');

    if (separatorIndex < 0)
    {
        return;
    }

    String key = line.substring(0, separatorIndex);
    String value = line.substring(separatorIndex + 1);

    key.trim();
    value.trim();
    key.toUpperCase();

    if (key == "STATE")
    {
        value.toCharArray(_state, sizeof(_state));
        return;
    }

    if (key == "FREQ")
    {
        _frequency = value.toFloat();
        return;
    }

    if (key == "POS" || key == "POSITION")
    {
        _position = value.toFloat();
        return;
    }

    if (key == "CUR" || key == "CUR_MA" || key == "CURRENT")
    {
        _current = value.toFloat();
        return;
    }
}

void SlaveLink::resetRxBuffer()
{
    _rxIndex = 0U;
    _rxBuffer[0] = '\0';
}