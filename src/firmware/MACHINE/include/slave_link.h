#pragma once

#include <Arduino.h>

// --- SlaveLink -------------------------------------------------------------

class SlaveLink
{
public:
    SlaveLink();

    void begin(HardwareSerial& serial, uint32_t baudrate = 115200);
    void update();

    void sendStart();
    void sendStop();
    void sendSetFrequency(float frequencyHz);
    void sendGetStatus();

    bool isOnline() const;
    const char* getState() const;
    float getFrequency() const;
    float getPosition() const;
    float getCurrent() const;

private:
    void sendLine(const String& line);
    void handleLine(const String& line);
    void resetRxBuffer();

private:
    HardwareSerial* _serial;
    bool _online;

    char _rxBuffer[64];
    size_t _rxIndex;

    char _state[16];
    float _frequency;
    float _position;
    float _current;

    unsigned long _lastRxMs;
    unsigned long _lastPollMs;
};