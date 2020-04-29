#pragma once

#include <iostream>
#include <string>

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
}

#include "luawrapper.hpp"
#include <ixwebsocket/IXWebSocket.h>
#include <chrono>
#include <thread>

using namespace ix;

WebSocket* WebSocket_new(lua_State* L)
{
    WebSocket* webSocket = new WebSocket();
    webSocket->setOnMessageCallback([](const WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message)
        {
            std::cout << msg->str << std::endl;
        }
    });
    return webSocket;
}

int WebSocket_getUrl(lua_State* L)
{
    WebSocket* webSocket = luaW_check<WebSocket>(L, 1);
    lua_pushstring(L, webSocket->getUrl().c_str());
    return 1;
}

int WebSocket_setUrl(lua_State* L)
{
    WebSocket* webSocket = luaW_check<WebSocket>(L, 1);
    const char* url = luaL_checkstring(L, 2);
    webSocket->setUrl(url);
    return 0;
}

int WebSocket_start(lua_State* L)
{
    WebSocket* webSocket = luaW_check<WebSocket>(L, 1);
    webSocket->start();
    return 0;
}

int WebSocket_send(lua_State* L)
{
    WebSocket* webSocket = luaW_check<WebSocket>(L, 1);
    const char* msg = luaL_checkstring(L, 2);
    webSocket->send(msg);
    return 0;
}

int WebSocket_sleep(lua_State* L)
{
    WebSocket* webSocket = luaW_check<WebSocket>(L, 1);
    int duration = luaL_checkinteger(L, 2);
    std::this_thread::sleep_for(std::chrono::milliseconds(duration));
    return 0;
}

static luaL_Reg WebSocket_table[] = {
    { NULL, NULL }
};

static luaL_Reg WebSocket_metatable[] = {
    { "getUrl", WebSocket_getUrl },
    { "setUrl", WebSocket_setUrl },
    { "start", WebSocket_start },
    { "send", WebSocket_send },
    { "sleep", WebSocket_sleep },
    { NULL, NULL }
};

int luaopen_WebSocket(lua_State* L)
{
    luaW_register<WebSocket>(L,
        "WebSocket",
        WebSocket_table,
        WebSocket_metatable,
        WebSocket_new
        );
    return 1;
}
