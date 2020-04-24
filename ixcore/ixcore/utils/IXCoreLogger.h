/*
 *  IXCoreLogger.h
 *  Author: Thomas Wells, Benjamin Sergeant
 *  Copyright (c) 2019-2020 Machine Zone, Inc. All rights reserved.
 */

#pragma once
#include <functional>
#include <string>

namespace ix
{
    enum class LogLevel
    {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3,
        Critical = 4
    };

    class CoreLogger
    {
    public:
        using LogFunc = std::function<void(const char*, LogLevel level)>;

        static void log(const char* msg, LogLevel level = LogLevel::Debug);

        static void debug(const std::string& msg);
        static void info(const std::string& msg);
        static void warn(const std::string& msg);
        static void error(const std::string& msg);
        static void critical(const std::string& msg);

        static void setLogFunction(LogFunc& func)
        {
            _currentLogger = func;
        }

    private:
        static LogFunc _currentLogger;
    };

} // namespace ix
