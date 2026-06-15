#pragma once

#include <Arduino.h>

// --- Constants -------------------------------------------------------------

constexpr size_t SERIAL_PARSER_MAX_LINE_LENGTH = 64;
constexpr size_t SERIAL_PARSER_QUEUE_SIZE = 4;

// --- SerialParser ----------------------------------------------------------

class SerialParser
{
public:
    SerialParser();

    void begin();
    void update(Stream& stream);

    bool hasLine() const;
    String popLine();

private:
    void resetRxBuffer();
    void pushLine(const char* line);
    bool queueIsEmpty() const;
    bool queueIsFull() const;

private:
    char _rxBuffer[SERIAL_PARSER_MAX_LINE_LENGTH];
    size_t _rxIndex;

    String _lineQueue[SERIAL_PARSER_QUEUE_SIZE];
    size_t _queueHead;
    size_t _queueTail;
    size_t _queueCount;
};