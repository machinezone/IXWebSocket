/*
 *  IXLog.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 *
 *  A minimal, dependency free logger used by the ws utility and by the
 *  unittest. It supports the {} placeholder syntax, writing to stdout/stderr
 *  or to a log file, and is safe to call from multiple threads.
 */

#pragma once

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace ix
{
    namespace logger
    {
        enum class Level
        {
            Debug = 0,
            Info = 1,
            Warn = 2,
            Error = 3
        };

        namespace detail
        {
            struct State
            {
                std::mutex mutex;
                Level level = Level::Debug;
                std::ofstream file;
            };

            inline State& state()
            {
                static State state;
                return state;
            }

            // No argument left, the rest of the format string is written as is.
            inline void format(std::ostream& os, const char* fmt)
            {
                os << fmt;
            }

            template<typename T, typename... Args>
            void format(std::ostream& os, const char* fmt, const T& value, const Args&... args)
            {
                for (; *fmt != '\0'; ++fmt)
                {
                    if (fmt[0] == '{' && fmt[1] == '}')
                    {
                        os << value;
                        format(os, fmt + 2, args...);
                        return;
                    }

                    os << *fmt;
                }
            }

            inline const char* levelName(Level level)
            {
                switch (level)
                {
                    case Level::Debug: return "debug";
                    case Level::Warn: return "warning";
                    case Level::Error: return "error";
                    case Level::Info:
                    default: return "info";
                }
            }

            inline std::string timestamp()
            {
                auto now = std::chrono::system_clock::now();
                auto time = std::chrono::system_clock::to_time_t(now);

                // std::localtime is the only portable option (localtime_r and
                // localtime_s are not available everywhere). It returns a
                // pointer to a shared buffer, which is fine here since all our
                // callers hold the logger mutex.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
                struct tm tm = *std::localtime(&time);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
                auto ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) %
                    1000;

                char buffer[32];
                std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);

                std::stringstream ss;
                ss << buffer << '.';
                ss.fill('0');
                ss.width(3);
                ss << ms.count();
                return ss.str();
            }

            template<typename... Args>
            void write(Level level, const std::string& fmt, const Args&... args)
            {
                auto& s = state();
                std::lock_guard<std::mutex> lock(s.mutex);

                if (level < s.level) return;

                std::stringstream ss;
                format(ss, fmt.c_str(), args...);

                std::ostream& os = s.file.is_open()
                                       ? static_cast<std::ostream&>(s.file)
                                       : (level == Level::Error ? std::cerr : std::cout);

                os << '[' << timestamp() << "] [" << levelName(level) << "] " << ss.str()
                   << std::endl;
            }
        } // namespace detail

        inline void setLevel(Level level)
        {
            auto& s = detail::state();
            std::lock_guard<std::mutex> lock(s.mutex);
            s.level = level;
        }

        // Redirect all logs to a file. Returns false if the file cannot be opened.
        inline bool setLogFile(const std::string& path)
        {
            auto& s = detail::state();
            std::lock_guard<std::mutex> lock(s.mutex);

            s.file.open(path.c_str(), std::ios::out | std::ios::trunc);
            return s.file.is_open();
        }
    } // namespace logger

    template<typename... Args>
    void logDebug(const std::string& fmt, const Args&... args)
    {
        logger::detail::write(logger::Level::Debug, fmt, args...);
    }

    template<typename... Args>
    void logInfo(const std::string& fmt, const Args&... args)
    {
        logger::detail::write(logger::Level::Info, fmt, args...);
    }

    template<typename... Args>
    void logWarn(const std::string& fmt, const Args&... args)
    {
        logger::detail::write(logger::Level::Warn, fmt, args...);
    }

    template<typename... Args>
    void logError(const std::string& fmt, const Args&... args)
    {
        logger::detail::write(logger::Level::Error, fmt, args...);
    }
} // namespace ix
