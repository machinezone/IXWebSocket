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

int main(int argc, char* argv[])
{
    ix::initNetSystem();

    ix::IXCoreLogger::LogFunc logFunc = [](const char* msg) { spdlog::info(msg); };
    ix::IXCoreLogger::setLogFunction(logFunc);

    int result = Catch::Session().run(argc, argv);

    ix::uninitNetSystem();
    return result;
}
