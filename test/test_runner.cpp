/*
 *  test_runner.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <ixwebsocket/IXNetSystem.h>

int main(int argc, char* argv[])
{
    ix::initNetSystem();

    int result = Catch::Session().run(argc, argv);

    ix::uninitNetSystem();
    return result;
}
