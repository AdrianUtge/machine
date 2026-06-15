#pragma once

#include <Arduino.h>

#include "motor_homing.h"

// --- Machine enums ---------------------------------------------------------

enum class MachineMode
{
    IDLE = 0,
    HOMING,
    READY,
    RUNNING,
    ERROR
};

// --- MachineState ----------------------------------------------------------

class MachineState
{
public:
    MachineState();

    void begin();
    void update();
    void stop(); 
    void home();
    void start();
    void hardReset();

    bool setSpeed(int32_t speed);
    bool setFrequency(float frequency);

    bool isHomed() const;
    MachineMode getMode() const;

    const char* stateToString() const;

    int32_t getSpeed() const;
    float getFrequency() const;
    float getPosition() const;
    float getCurrent() const;
    float getForce() const;
    const char* getSlaveStatus() const;

private:
    void resetMeasurements();
    void refreshHx711Status();
    bool calibrateLoadCellsEmpty();
    bool updateForceMeasurement();
    float readLoadCellRawAverage(uint8_t samples);
    void setErrorState();

private:
    MachineMode _mode;
    bool _homed;

    int32_t _speed;
    float _frequency;

    float _position;
    float _current;
    float _force;

    long _loadCellOffset;
    float _loadCellRaw;
    bool _hx711Ready;

    bool _slaveOnline;

    unsigned long _lastUpdateMs;

    MotorHoming _motor;
};