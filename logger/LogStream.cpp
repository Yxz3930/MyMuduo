#include "LogStream.h"

std::string LogStream::str()
{
    return std::string(this->buffer.data(), this->buffer.size());
}

void LogStream::clear()
{
    this->buffer.clear();
}
