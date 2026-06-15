/*
OpenRB-150 modular firmware
Dynamixel homing module
*/

#include "motor_homing.h"

#include <Dynamixel2Arduino.h>

// --- Board serial config ---------------------------------------------------

#if defined(ARDUINO_OpenRB)
    #define DXL_SERIAL Serial1
    const int DXL_DIR_PIN = -1;
#else
    #define DXL_SERIAL Serial1
    const int DXL_DIR_PIN = 2;
#endif

using namespace ControlTableItem;

// --- User settings ---------------------------------------------------------

static constexpr uint8_t DXL_ID = 1;
static constexpr float DXL_PROTOCOL_VER = 2.0f;
static constexpr uint32_t DXL_BAUDRATE = 57600;

static constexpr float TABLE_LENGTH_MM = 960.0f;

static constexpr int32_t KNOWN_TRAVEL_TICKS = 166948;
static constexpr float KNOWN_TICKS_PER_MM = 173.904160f;

static constexpr int32_t HOMING_STEP_TICKS = 20;
static constexpr int32_t BACKOFF_TICKS = 80;

static constexpr int CURRENT_LIMIT_MA = 180;
static constexpr int BLOCK_COUNT_LIMIT = 8;
static constexpr int HOMING_DELAY_MS = 40;

static constexpr int NUMBER_OF_TAPS = 5;

static constexpr float BOTTOM_SLOW_ZONE_MM = 1.0f;

static constexpr int32_t SLOW_PROFILE_VELOCITY = 70;
static constexpr int32_t SLOW_PROFILE_ACCELERATION = 10;

static constexpr int32_t FAST_PROFILE_VELOCITY = 250;
static constexpr int32_t FAST_PROFILE_ACCELERATION = 60;

static constexpr int32_t POSITION_TOLERANCE = 10;

static constexpr int32_t HIGH_SPEED_THRESHOLD_RAW = 100;
static constexpr int32_t LOW_SPEED_THRESHOLD_RAW = 10;
static constexpr uint32_t DECEL_BLOCK_IGNORE_MS = 1000;

// --- File globals ----------------------------------------------------------

static Dynamixel2Arduino g_dxl(DXL_SERIAL, DXL_DIR_PIN);

// --- Constructor -----------------------------------------------------------

MotorHoming::MotorHoming()
    : _debug(nullptr),
      _ready(false),
      _calibrated(false),
      _bottomTick(0),
      _topTick(0),
      _ticksPerMm(0.0f),
      _lastVelocityRaw(0),
      _blockIgnoreUntil(0)
{
}

// --- Public API ------------------------------------------------------------

bool MotorHoming::begin(Stream& debug)
{
    _debug = &debug;

    g_dxl.begin(DXL_BAUDRATE);
    g_dxl.setPortProtocolVersion(DXL_PROTOCOL_VER);

    delay(500);

    if (!g_dxl.ping(DXL_ID))
    {
        if (_debug != nullptr)
        {
            _debug->println("# ERROR: Dynamixel not detected.");
        }

        _ready = false;
        return false;
    }

    configureMotor();

    _ready = true;
    _calibrated = false;

    if (_debug != nullptr)
    {
        _debug->println("# Dynamixel ready");
    }

    return true;
}

bool MotorHoming::doHoming()
{
    if (!_ready)
    {
        if (_debug != nullptr)
        {
            _debug->println("# ERROR: motor not ready");
        }

        return false;
    }

    if (_debug != nullptr)
    {
        _debug->println("# Starting top-first homing...");
        _debug->println("# Assumption: carrier starts near the upper limit.");
    }

    configureMotor();

    _topTick = findLimitMultiTap(1);

    if (_debug != nullptr)
    {
        _debug->print("# top_tick = ");
        _debug->println(_topTick);
    }

    delay(500);

    const int32_t estimatedBottomTick = _topTick - KNOWN_TRAVEL_TICKS;
    const int32_t fastTarget = estimatedBottomTick + static_cast<int32_t>(BOTTOM_SLOW_ZONE_MM * KNOWN_TICKS_PER_MM);

    if (_debug != nullptr)
    {
        _debug->print("# estimated_bottom_tick = ");
        _debug->println(estimatedBottomTick);

        _debug->print("# fast approach target = ");
        _debug->println(fastTarget);

        _debug->println("# Fast move near bottom...");
    }

    setMotionProfile(FAST_PROFILE_VELOCITY, FAST_PROFILE_ACCELERATION);
    setGoalPosition(fastTarget);

    waitUntilReached(fastTarget, 20000);

    delay(300);

    _lastVelocityRaw = getVelocityRaw();

    delay(500);

    _bottomTick = findLimitMultiTap(-1);

    if (_debug != nullptr)
    {
        _debug->print("# bottom_tick = ");
        _debug->println(_bottomTick);
    }

    const int32_t travelTicks = _topTick - _bottomTick;

    if (abs(travelTicks) < 100)
    {
        if (_debug != nullptr)
        {
            _debug->println("# ERROR: travel too small. Homing failed.");
        }

        _calibrated = false;
        return false;
    }

    _ticksPerMm = static_cast<float>(travelTicks) / TABLE_LENGTH_MM;
    _calibrated = true;

    if (_debug != nullptr)
    {
        _debug->println("# Homing complete");

        _debug->print("# bottom_tick = ");
        _debug->println(_bottomTick);

        _debug->print("# top_tick = ");
        _debug->println(_topTick);

        _debug->print("# travel_ticks = ");
        _debug->println(travelTicks);

        _debug->print("# ticks_per_mm = ");
        _debug->println(_ticksPerMm, 6);
    }

    gotoMm(0.0f);
    waitUntilReached(_bottomTick, 10000);

    return true;
}

bool MotorHoming::isCalibrated() const
{
    return _calibrated;
}

void MotorHoming::gotoMm(float mm)
{
    if (!_calibrated)
    {
        if (_debug != nullptr)
        {
            _debug->println("# ERROR: not calibrated. Run HOME first.");
        }

        return;
    }

    mm = constrain(mm, 0.0f, TABLE_LENGTH_MM);

    const int32_t goal = mmToTicks(mm);

    if (_debug != nullptr)
    {
        _debug->print("# Moving to ");
        _debug->print(mm, 2);
        _debug->print(" mm -> tick ");
        _debug->println(goal);
    }

    setMotionProfile(FAST_PROFILE_VELOCITY, FAST_PROFILE_ACCELERATION);
    setGoalPosition(goal);
}

float MotorHoming::getPositionMm() const
{
    return ticksToMm(getPositionTicks());
}

int32_t MotorHoming::getPositionTicks() const
{
    return g_dxl.readControlTableItem(PRESENT_POSITION, DXL_ID);
}

int32_t MotorHoming::getVelocityRaw() const
{
    return g_dxl.readControlTableItem(PRESENT_VELOCITY, DXL_ID);
}

int16_t MotorHoming::getCurrentRaw() const
{
    const int32_t raw = g_dxl.readControlTableItem(PRESENT_CURRENT, DXL_ID);
    return static_cast<int16_t>(raw & 0xFFFF);
}

void MotorHoming::torqueOn()
{
    g_dxl.writeControlTableItem(TORQUE_ENABLE, DXL_ID, 1);
}

void MotorHoming::torqueOff()
{
    g_dxl.writeControlTableItem(TORQUE_ENABLE, DXL_ID, 0);
}

// --- Motor config ----------------------------------------------------------

void MotorHoming::configureMotor()
{
    torqueOff();

    g_dxl.writeControlTableItem(OPERATING_MODE, DXL_ID, 4);

    torqueOn();

    setMotionProfile(SLOW_PROFILE_VELOCITY, SLOW_PROFILE_ACCELERATION);
}

void MotorHoming::setMotionProfile(int32_t velocity, int32_t acceleration)
{
    g_dxl.writeControlTableItem(PROFILE_VELOCITY, DXL_ID, velocity);
    g_dxl.writeControlTableItem(PROFILE_ACCELERATION, DXL_ID, acceleration);
}

void MotorHoming::setGoalPosition(int32_t goal)
{
    g_dxl.writeControlTableItem(GOAL_POSITION, DXL_ID, goal);
}

// --- Homing helpers --------------------------------------------------------

bool MotorHoming::waitUntilReached(int32_t target, uint32_t timeoutMs)
{
    const uint32_t t0 = millis();

    while ((millis() - t0) < timeoutMs)
    {
        const int32_t pos = getPositionTicks();

        if (abs(pos - target) < POSITION_TOLERANCE)
        {
            return true;
        }

        delay(20);
    }

    return false;
}

void MotorHoming::resetBlockDetection()
{
    for (int i = 0; i < 15; i++)
    {
        getCurrentRaw();
        delay(10);
    }

    _lastVelocityRaw = getVelocityRaw();
    _blockIgnoreUntil = 0;
}

bool MotorHoming::isBlocked()
{
    static int blockCount = 0;

    const int32_t currentVelocityRaw = getVelocityRaw();

    const bool wasFast = abs(_lastVelocityRaw) > HIGH_SPEED_THRESHOLD_RAW;
    const bool nowSlow = abs(currentVelocityRaw) < LOW_SPEED_THRESHOLD_RAW;

    if (wasFast && nowSlow)
    {
        _blockIgnoreUntil = millis() + DECEL_BLOCK_IGNORE_MS;
        blockCount = 0;

        if (_debug != nullptr)
        {
            _debug->println("# Deceleration detected -> block detection buffered");
        }
    }

    _lastVelocityRaw = currentVelocityRaw;

    if (millis() < _blockIgnoreUntil)
    {
        blockCount = 0;
        return false;
    }

    const int16_t currentMa = abs(getCurrentRaw());

    if (currentMa > CURRENT_LIMIT_MA)
    {
        blockCount++;
    }
    else
    {
        blockCount = 0;
    }

    return blockCount >= BLOCK_COUNT_LIMIT;
}

int32_t MotorHoming::findLimitOneTap(int direction)
{
    setMotionProfile(SLOW_PROFILE_VELOCITY, SLOW_PROFILE_ACCELERATION);
    resetBlockDetection();

    int32_t goal = getPositionTicks();

    while (true)
    {
        goal += direction * HOMING_STEP_TICKS;
        setGoalPosition(goal);

        delay(HOMING_DELAY_MS);

        const int16_t currentMa = abs(getCurrentRaw());
        const int32_t pos = getPositionTicks();
        const int32_t vel = getVelocityRaw();

        if (_debug != nullptr)
        {
            _debug->print("# tap pos=");
            _debug->print(pos);
            _debug->print(" vel=");
            _debug->print(vel);
            _debug->print(" current_mA=");
            _debug->println(currentMa);
        }

        if (isBlocked())
        {
            if (_debug != nullptr)
            {
                _debug->println("# Limit tap detected");
            }

            const int32_t limitPos = getPositionTicks();

            const int32_t backoff = limitPos - direction * BACKOFF_TICKS;
            setGoalPosition(backoff);
            waitUntilReached(backoff, 3000);

            return limitPos;
        }
    }
}

int32_t MotorHoming::findLimitMultiTap(int direction)
{
    if (_debug != nullptr)
    {
        _debug->println(direction > 0 ? "# Searching upper limit with 3 taps..." : "# Searching lower limit with 3 taps...");
    }

    int64_t sum = 0;

    for (int i = 0; i < NUMBER_OF_TAPS; i++)
    {
        if (_debug != nullptr)
        {
            _debug->print("# Tap ");
            _debug->print(i + 1);
            _debug->print("/");
            _debug->println(NUMBER_OF_TAPS);
        }

        const int32_t tapPos = findLimitOneTap(direction);
        sum += tapPos;

        delay(300);
    }

    const int32_t averagedLimit = static_cast<int32_t>(sum / NUMBER_OF_TAPS);

    if (_debug != nullptr)
    {
        _debug->print("# Averaged limit = ");
        _debug->println(averagedLimit);
    }

    const int32_t finalBackoff = averagedLimit - direction * BACKOFF_TICKS;
    setGoalPosition(finalBackoff);
    waitUntilReached(finalBackoff, 3000);

    return averagedLimit;
}

// --- Mapping ---------------------------------------------------------------

int32_t MotorHoming::mmToTicks(float mm) const
{
    mm = constrain(mm, 0.0f, TABLE_LENGTH_MM);
    return _bottomTick + static_cast<int32_t>(mm * _ticksPerMm);
}

float MotorHoming::ticksToMm(int32_t ticks) const
{
    if (!_calibrated || abs(_ticksPerMm) < 1e-6f)
    {
        return 0.0f;
    }

    return static_cast<float>(ticks - _bottomTick) / _ticksPerMm;
}