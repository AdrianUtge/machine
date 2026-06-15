#include "serial_parser.h"

// --- Constructor -----------------------------------------------------------

SerialParser::SerialParser()
    : _rxIndex(0),
      _queueHead(0),
      _queueTail(0),
      _queueCount(0)
{
    resetRxBuffer();
}

// --- Public API ------------------------------------------------------------

void SerialParser::begin()
{
    _rxIndex = 0;
    _queueHead = 0;
    _queueTail = 0;
    _queueCount = 0;
    resetRxBuffer();
}

void SerialParser::update(Stream& stream)
{
    while (stream.available() > 0)
    {
        const char c = static_cast<char>(stream.read());

        if (c == '\r')
        {
            continue;
        }

        if (c == '\n')
        {
            _rxBuffer[_rxIndex] = '\0';

            if (_rxIndex > 0)
            {
                pushLine(_rxBuffer);
            }

            resetRxBuffer();
            continue;
        }

        if (_rxIndex < (SERIAL_PARSER_MAX_LINE_LENGTH - 1))
        {
            _rxBuffer[_rxIndex++] = c;
            _rxBuffer[_rxIndex] = '\0';
        }
        else
        {
            resetRxBuffer();
        }
    }
}

bool SerialParser::hasLine() const
{
    return !queueIsEmpty();
}

String SerialParser::popLine()
{
    if (queueIsEmpty())
    {
        return String("");
    }

    const String line = _lineQueue[_queueHead];
    _queueHead = (_queueHead + 1U) % SERIAL_PARSER_QUEUE_SIZE;
    _queueCount--;

    return line;
}

// --- Private helpers -------------------------------------------------------

void SerialParser::resetRxBuffer()
{
    _rxIndex = 0;
    _rxBuffer[0] = '\0';
}

void SerialParser::pushLine(const char* line)
{
    if (queueIsFull())
    {
        return;
    }

    _lineQueue[_queueTail] = String(line);
    _queueTail = (_queueTail + 1U) % SERIAL_PARSER_QUEUE_SIZE;
    _queueCount++;
}

bool SerialParser::queueIsEmpty() const
{
    return (_queueCount == 0U);
}

bool SerialParser::queueIsFull() const
{
    return (_queueCount >= SERIAL_PARSER_QUEUE_SIZE);
}