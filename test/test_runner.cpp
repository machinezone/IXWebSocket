/*
 *  test_runner.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

int main(int argc, char* argv[]) 
{
    int result = Catch::Session().run(argc, argv);
    return result;
}
