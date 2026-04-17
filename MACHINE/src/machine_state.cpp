

#include "machine_state.h"

// --- Constants -------------------------------------------------------------

static constexpr int32_t MACHINE_MIN_SPEED = 0;
static constexpr int32_t MACHINE_MAX_SPEED = 10000;

static constexpr float MACHINE_MIN_FREQ = 0.0f;
static constexpr float MACHINE_MAX_FREQ = 1000.0f;

// --- Constructor -----------------------------------------------------------

MachineState::MachineState()
    : _mode(MachineMode::IDLE),
      _homed(false),
      _speed(100),
      _frequency(0.8f),
      _position(0.0f),
      _current(0.0f),
      _force(0.0f),
      _slaveOnline(true),
      _lastUpdateMs(0U)
{
}

// --- Public API ------------------------------------------------------------

void MachineState::begin()
{
    _mode = MachineMode::IDLE;
    _homed = false;
    _speed = 100;
    _frequency = 0.8f;
    resetMeasurements();
    _slaveOnline = true;
    _lastUpdateMs = millis();
}

void MachineState::update()
{
    const unsigned long now = millis();

    if ((now - _lastUpdateMs) < 20U)
    {
        return;
    }

    _lastUpdateMs = now;

    if (_mode == MachineMode::RUNNING)
    {
        _position += (_speed * _frequency) * 0.001f;
        _current = 0.25f + (static_cast<float>(_speed) * 0.002f);
        _force = 1.0f + (_frequency * 2.5f);
    }
    else if (_mode == MachineMode::HOMING)
    {
        _position = 0.0f;
        _current = 0.2f;
        _force = 0.0f;

        _homed = true;
        _mode = MachineMode::READY;
    }
    else
    {
        _current = 0.1f;
        _force = 0.0f;
    }
}

void MachineState::home()
{
    _mode = MachineMode::HOMING;
}

void MachineState::start()
{
    if (_homed)
    {
        _mode = MachineMode::RUNNING;
    }
    else
    {
        _mode = MachineMode::ERROR;
    }
}

void MachineState::hardReset()
{
    _mode = MachineMode::IDLE;
    _homed = false;
    _speed = 100;
    _frequency = 0.8f;
    resetMeasurements();
    _slaveOnline = true;
}

bool MachineState::setSpeed(int32_t speed)
{
    if ((speed < MACHINE_MIN_SPEED) || (speed > MACHINE_MAX_SPEED))
    {
        return false;
    }

    _speed = speed;
    return true;
}

bool MachineState::setFrequency(float frequency)
{
    if ((frequency < MACHINE_MIN_FREQ) || (frequency > MACHINE_MAX_FREQ))
    {
        return false;
    }

    _frequency = frequency;
    return true;
}

bool MachineState::isHomed() const
{
    return _homed;
}

const char* MachineState::stateToString() const
{
    switch (_mode)
    {
        case MachineMode::IDLE:
            return "IDLE";

        case MachineMode::HOMING:
            return "HOMING";

        case MachineMode::READY:
            return "READY";

        case MachineMode::RUNNING:
            return "RUNNING";

        case MachineMode::ERROR:
            return "ERROR";

        default:
            return "UNKNOWN";
    }
}

int32_t MachineState::getSpeed() const
{
    return _speed;
}

float MachineState::getFrequency() const
{
    return _frequency;
}

float MachineState::getPosition() const
{
    return _position;
}

float MachineState::getCurrent() const
{
    return _current;
}

float MachineState::getForce() const
{
    return _force;
}

const char* MachineState::getSlaveStatus() const
{
    return _slaveOnline ? "ONLINE" : "OFFLINE";
}

// --- Private helpers -------------------------------------------------------

void MachineState::resetMeasurements()
{
    _position = 0.0f;
    _current = 0.0f;
    _force = 0.0f;
}