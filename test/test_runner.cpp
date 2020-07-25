/*
 *  test_runner.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include <ixcore/utils/IXCoreLogger.h>
#include <ixwebsocket/IXNetSystem.h>
#include <spdlog/spdlog.h>

#ifndef _WIN32
#include <signal.h>
#endif

int main(int argc, char* argv[])
{
    ix::initNetSystem();

#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    ix::CoreLogger::LogFunc logFunc = [](const char* msg, ix::LogLevel level) {
        switch (level)
        {
            case ix::LogLevel::Debug:
            {
                spdlog::debug(msg);
            }
            break;

            case ix::LogLevel::Info:
            {
                spdlog::info(msg);
            }
            break;

            case ix::LogLevel::Warning:
            {
                spdlog::warn(msg);
            }
            break;

            case ix::LogLevel::Error:
            {
                spdlog::error(msg);
            }
            break;

            case ix::LogLevel::Critical:
            {
                spdlog::critical(msg);
            }
            break;
        }
    };
    ix::CoreLogger::setLogFunction(logFunc);
    spdlog::set_level(spdlog::level::debug);

    int result = Catch::Session().run(argc, argv);

    ix::uninitNetSystem();
    return result;
}
