/*
 *  functions.hpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <iostream>

int lua_info(lua_State* /*L*/)
{
    std::cout << "C++ Lua v0.1" << std::endl << std::endl;
    return 0;
}
