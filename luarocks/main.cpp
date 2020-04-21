#include <iostream>

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include "functions.hpp"
#include "LuaPlayer.hpp"

int main()
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    // Functions
    lua_register(L, "info", lua_info);

    // Objets
    luaopen_Player(L);

    luaL_dofile(L, "ia.lua");
    lua_close(L);
}

