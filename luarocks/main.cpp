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
    luaopen_WebSocket(L);

#if 0
    luaL_dofile(L, "ia.lua");
#else
    int loadStatus = luaL_loadfile(L, "ia.lua");
    if (loadStatus)
    {
        std::cerr << "Error loading gof_server.lua" << std::endl;
        std::cerr << lua_tostring(L, -1) << std::endl;
        return 1;
    }
    else
    {
        std::cout << "loaded ia.lua" << std::endl;
    }

    try {
        lua_call(L, 0, 0);
    }
    catch (const std::runtime_error& ex) {
        std::cerr << ex.what() << std::endl;
    }
#endif
    lua_close(L);
}
