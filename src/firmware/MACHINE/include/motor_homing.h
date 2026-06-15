#pragma once

#include <Arduino.h>

// --- MotorHoming -----------------------------------------------------------

class MotorHoming
{
public:
    MotorHoming();

    bool begin(Stream& debug);
    bool doHoming();

    bool isCalibrated() const;

    void gotoMm(float mm);
    float getPositionMm() const;
    int32_t getPositionTicks() const;
    int32_t getVelocityRaw() const;
    int16_t getCurrentRaw() const;

    void torqueOn();
    void torqueOff();

private:
    void configureMotor();
    void setMotionProfile(int32_t velocity, int32_t acceleration);
    void setGoalPosition(int32_t goal);

    bool waitUntilReached(int32_t target, uint32_t timeoutMs);
    void resetBlockDetection();
    bool isBlocked();

    int32_t findLimitOneTap(int direction);
    int32_t findLimitMultiTap(int direction);

    int32_t mmToTicks(float mm) const;
    float ticksToMm(int32_t ticks) const;

private:
    Stream* _debug;

    bool _ready;
    bool _calibrated;

    int32_t _bottomTick;
    int32_t _topTick;
    float _ticksPerMm;

    int32_t _lastVelocityRaw;
    uint32_t _blockIgnoreUntil;
};