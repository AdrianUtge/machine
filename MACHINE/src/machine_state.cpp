/*
OpenRB-150 modular firmware
Centralized machine state
*/

#include "machine_state.h"

#include "HX711.h"

// --- Constants -------------------------------------------------------------

static constexpr int32_t MACHINE_MIN_SPEED = 0;
static constexpr int32_t MACHINE_MAX_SPEED = 10000;

static constexpr float MACHINE_MIN_FREQ = 0.0f;
static constexpr float MACHINE_MAX_FREQ = 1000.0f;

static constexpr uint8_t HX711_DT_PIN = 6;
static constexpr uint8_t HX711_SCK_PIN = 9;

static constexpr uint8_t HX711_TARE_SAMPLES = 20;
static constexpr uint8_t HX711_READ_SAMPLES = 10;

// --- File globals ----------------------------------------------------------

static HX711 g_scale;

// --- Constructor -----------------------------------------------------------

MachineState::MachineState()
    : _mode(MachineMode::IDLE),
      _homed(false),
      _speed(100),
      _frequency(0.8f),
      _position(0.0f),
      _current(0.0f),
      _force(0.0f),
      _loadCellOffset(0),
      _loadCellRaw(0.0f),
      _hx711Ready(false),
      _slaveOnline(false),
      _lastUpdateMs(0U),
      _motor()
{
}

// --- Public API ------------------------------------------------------------

void MachineState::begin()
{
    g_scale.begin(HX711_DT_PIN, HX711_SCK_PIN);

    _mode = MachineMode::IDLE;
    _homed = false;
    _speed = 100;
    _frequency = 0.8f;
    _slaveOnline = false;
    _lastUpdateMs = millis();

    resetMeasurements();

    const bool motorReady = _motor.begin(Serial);
    refreshHx711Status();

    if (_hx711Ready)
    {
        calibrateLoadCellsEmpty();
    }

    if (!motorReady)
    {
        setErrorState();
    }
}

void MachineState::update()
{
    const unsigned long now = millis();

    if ((now - _lastUpdateMs) < 20U)
    {
        return;
    }

    _lastUpdateMs = now;

    refreshHx711Status();

    if (_motor.isCalibrated())
    {
        _position = _motor.getPositionMm();
        _current = static_cast<float>(abs(_motor.getCurrentRaw()));
    }

    if (_mode == MachineMode::RUNNING)
    {
        if (!updateForceMeasurement())
        {
            setErrorState();
            return;
        }
    }
    else if (_mode == MachineMode::READY)
    {
        if (_hx711Ready)
        {
            updateForceMeasurement();
        }
    }
    else if (_mode == MachineMode::IDLE)
    {
        _force = 0.0f;
    }
}

void MachineState::home()
{
    if (_mode == MachineMode::ERROR)
    {
        return;
    }

    _mode = MachineMode::HOMING;

    const bool homingOk = _motor.doHoming();

    if (!homingOk)
    {
        _homed = false;
        setErrorState();
        return;
    }

    _homed = true;
    _position = _motor.getPositionMm();
    _current = static_cast<float>(abs(_motor.getCurrentRaw()));
    _mode = MachineMode::READY;
}

void MachineState::start()
{
    if (!_homed)
    {
        setErrorState();
        return;
    }

    if (!_hx711Ready)
    {
        setErrorState();
        return;
    }

    if (!calibrateLoadCellsEmpty())
    {
        setErrorState();
        return;
    }

    _mode = MachineMode::RUNNING;
}

void MachineState::stop()
{
    _motor.torqueOff();
    _mode = MachineMode::IDLE;
}

void MachineState::hardReset()
{
    _mode = MachineMode::IDLE;
    _homed = false;
    _speed = 100;
    _frequency = 0.8f;
    _slaveOnline = false;

    refreshHx711Status();
    resetMeasurements();

    if (_hx711Ready)
    {
        calibrateLoadCellsEmpty();
    }
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

MachineMode MachineState::getMode() const
{
    return _mode;
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
    _loadCellRaw = 0.0f;
    _loadCellOffset = 0;
}

void MachineState::refreshHx711Status()
{
    _hx711Ready = g_scale.is_ready();
}

bool MachineState::calibrateLoadCellsEmpty()
{
    refreshHx711Status();

    if (!_hx711Ready)
    {
        return false;
    }

    _loadCellOffset = static_cast<long>(readLoadCellRawAverage(HX711_TARE_SAMPLES));
    _force = 0.0f;

    return true;
}

bool MachineState::updateForceMeasurement()
{
    refreshHx711Status();

    if (!_hx711Ready)
    {
        return false;
    }

    _loadCellRaw = readLoadCellRawAverage(HX711_READ_SAMPLES);
    _force = _loadCellRaw - static_cast<float>(_loadCellOffset);

    return true;
}

float MachineState::readLoadCellRawAverage(uint8_t samples)
{
    return static_cast<float>(g_scale.read_average(samples));
}

void MachineState::setErrorState()
{
    _mode = MachineMode::ERROR;
}