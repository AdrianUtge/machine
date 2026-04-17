#pragma once

#include <Arduino.h>

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

    void home();
    void start();
    void hardReset();

    bool setSpeed(int32_t speed);
    bool setFrequency(float frequency);

    bool isHomed() const;

    const char* stateToString() const;

    int32_t getSpeed() const;
    float getFrequency() const;
    float getPosition() const;
    float getCurrent() const;
    float getForce() const;
    const char* getSlaveStatus() const;

private:
    void resetMeasurements();

private:
    MachineMode _mode;
    bool _homed;

    int32_t _speed;
    float _frequency;

    float _position;
    float _current;
    float _force;

    bool _slaveOnline;

    unsigned long _lastUpdateMs;
};