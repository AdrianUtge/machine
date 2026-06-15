/*
Slave Dynamixel controller
*/

#include "slave_dxl.h"
#include <Dynamixel2Arduino.h>

#if defined(ARDUINO_OpenRB)
    #define DXL_SERIAL Serial1
    const int DXL_DIR_PIN = -1;
#else
    #define DXL_SERIAL Serial1
    const int DXL_DIR_PIN = 2;
#endif

using namespace ControlTableItem;

static constexpr uint8_t DXL_ID = 1;
static constexpr float DXL_PROTOCOL_VER = 2.0f;
static constexpr uint32_t DXL_BAUDRATE = 57600;

static constexpr float FREQ_TO_VELOCITY = 50.0f;

static Dynamixel2Arduino dxl(DXL_SERIAL, DXL_DIR_PIN);

bool SlaveDXL::begin(Stream& debug)
{
    _debug = &debug;

    dxl.begin(DXL_BAUDRATE);
    dxl.setPortProtocolVersion(DXL_PROTOCOL_VER);

    delay(500);

    if (!dxl.ping(DXL_ID))
    {
        _debug->println("ERROR:DXL_NOT_FOUND");
        return false;
    }

    dxl.torqueOff(DXL_ID);
    dxl.setOperatingMode(DXL_ID, OP_VELOCITY);
    dxl.torqueOn(DXL_ID);

    _running = false;
    _frequency = 0.0f;
    _goalVelocity = 0;

    _debug->println("ACK:DXL_READY");

    return true;
}

void SlaveDXL::update()
{
    if (_running)
    {
        applyVelocity();
    }
}

void SlaveDXL::setFrequency(float freq)
{
    _frequency = freq;
    _goalVelocity = static_cast<int32_t>(freq * FREQ_TO_VELOCITY);
}

void SlaveDXL::start()
{
    _running = true;
}

void SlaveDXL::stop()
{
    _running = false;
    dxl.setGoalVelocity(DXL_ID, 0);
}

void SlaveDXL::hardReset()
{
    stop();
    _frequency = 0.0f;
}

void SlaveDXL::applyVelocity()
{
    dxl.setGoalVelocity(DXL_ID, _goalVelocity);
}

const char* SlaveDXL::getState() const
{
    return _running ? "RUNNING" : "STOPPED";
}

float SlaveDXL::getFrequency() const
{
    return _frequency;
}
