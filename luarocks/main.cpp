/*
 *  main.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>

extern "C"
{
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

#include "functions.hpp"
#include "LuaWebSocket.h"
#include <iostream>

int main()
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    // Functions
    lua_register(L, "info", lua_info);

    // Objets
    ix::luaopen_WebSocket(L);

    //
    // Simple version does little error handling
    // luaL_dofile(L, "ia.lua");
    //
    std::string luaFile("ia.lua");
    int loadStatus = luaL_loadfile(L, luaFile.c_str());
    if (loadStatus)
    {
        std::cerr << "Error loading " << luaFile << std::endl;
        std::cerr << lua_tostring(L, -1) << std::endl;
        return 1;
    }
    else
    {
        std::cout << "loaded " << luaFile << std::endl;
    }

    //
    // Capture lua errors
    //
    try
    {
        lua_call(L, 0, 0);
        lua_close(L);
    }
    catch (const std::runtime_error& ex)
    {
        lua_close(L);
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
