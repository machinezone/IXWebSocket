#ifndef FUNCTIONS_HPP
#define FUNCTIONS_HPP

#include <iostream>

int lua_info(lua_State* L)
{
    std::cout << "C++ Lua v0.1" << std::endl << std::endl;
    return 0;
}

#endif // FUNCTIONS_HPP