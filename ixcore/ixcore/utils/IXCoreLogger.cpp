/*
 *  IXCoreLogger.cpp
 *  Author: Thomas Wells, Benjamin Sergeant
 *  Copyright (c) 2019-2020 Machine Zone, Inc. All rights reserved.
 */

#include "ixcore/utils/IXCoreLogger.h"

namespace ix
{
    // Default do a no-op logger
    CoreLogger::LogFunc CoreLogger::_currentLogger = [](const char*, LogLevel) {};

    void CoreLogger::log(const char* msg, LogLevel level)
    {
        _currentLogger(msg, level);
    }

    void CoreLogger::debug(const std::string& msg)
    {
        _currentLogger(msg.c_str(), LogLevel::Debug);
    }

    void CoreLogger::info(const std::string& msg)
    {
        _currentLogger(msg.c_str(), LogLevel::Info);
    }

    void CoreLogger::warn(const std::string& msg)
    {
        _currentLogger(msg.c_str(), LogLevel::Warning);
    }

    void CoreLogger::error(const std::string& msg)
    {
        _currentLogger(msg.c_str(), LogLevel::Error);
    }

    void CoreLogger::critical(const std::string& msg)
    {
        _currentLogger(msg.c_str(), LogLevel::Critical);
    }

} // namespace ix
