#ifndef LUAPLAYER_HPP
#define LUAPLAYER_HPP

#include <iostream>
#include <string>

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
}

#include "luawrapper.hpp"
#include "Player.hpp"

Player* Player_new(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    unsigned int health = luaL_checkinteger(L, 2);
    return new Player(name, health);
}

int Player_getName(lua_State* L)
{
    Player* player = luaW_check<Player>(L, 1);
    lua_pushstring(L, player->getName());
    return 1;
}

int Player_getHealth(lua_State* L)
{
    Player* player = luaW_check<Player>(L, 1);
    lua_pushinteger(L, player->getHealth());
    return 1;
}

int Player_setHealth(lua_State* L)
{
    Player* player = luaW_check<Player>(L, 1);
    unsigned int health = luaL_checkinteger(L, 2);
    player->setHealth(health);
    return 0;
}

int Player_heal(lua_State* L)
{
    Player* player = luaW_check<Player>(L, 1);
    Player* target = luaW_check<Player>(L, 2);
    player->heal(target);
    return 0;
}

int Player_say(lua_State* L)
{
    Player* player = luaW_check<Player>(L, 1);
    const char* text = luaL_checkstring(L, 2);
    player->say(text);
    return 0;
}

int Player_info(lua_State* L)
{
    Player* player = luaW_check<Player>(L, 1);
    player->info();
    return 0;
}

static luaL_Reg Player_table[] = {
    { NULL, NULL }
};

static luaL_Reg Player_metatable[] = {
    { "getName", Player_getName },
    { "getHealth", Player_getHealth },
    { "setHealth", Player_setHealth },
    { "heal", Player_heal },
    { "say", Player_say },
    { "info", Player_info },
    { NULL, NULL }
};

int luaopen_Player(lua_State* L)
{
    luaW_register<Player>(L,
        "Player",
        Player_table,
        Player_metatable,
        Player_new
        );
    return 1;
}

#endif // LUAPLAYER_HPP