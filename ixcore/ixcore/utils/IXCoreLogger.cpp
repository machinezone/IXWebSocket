#include "ixcore/utils/IXCoreLogger.h"


namespace ix
{
// Default do nothing logger
IXCoreLogger::LogFunc IXCoreLogger::_currentLogger = [](const char* /*msg*/){};

void IXCoreLogger::Log(const char* msg)
{
    _currentLogger(msg);
}

} // ix
