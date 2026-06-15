/*
Slave Dynamixel controller (frequency driven)
*/

#pragma once

#include <Arduino.h>

class SlaveDXL
{
public:
    bool begin(Stream& debug);

    void update();

    void setFrequency(float freq);
    void start();
    void stop();
    void hardReset();

    const char* getState() const;
    float getFrequency() const;

private:
    void applyVelocity();

private:
    Stream* _debug;

    bool _running;
    float _frequency;

    int32_t _goalVelocity;
};
