/*
 *  LuaWebSocket.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

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
#include <queue>
#include <mutex>

namespace ix
{
    class LuaWebSocket
    {
        public:
            LuaWebSocket()
            {
                _webSocket.setOnMessageCallback([this](const WebSocketMessagePtr& msg) {
                    if (msg->type == ix::WebSocketMessageType::Message)
                    {
                        std::lock_guard<std::mutex> lock(_queueMutex);
                        _queue.push(msg->str);
                    }
                });
            }

            void setUrl(const std::string& url) { _webSocket.setUrl(url); }
            const std::string& getUrl() { return _webSocket.getUrl(); }
            void start() { _webSocket.start(); }
            void send(const std::string& msg) { _webSocket.send(msg); }

            const std::string& getMessage()
            {
                std::lock_guard<std::mutex> lock(_queueMutex);
                return _queue.front();
            }

            bool hasMessage()
            {
                std::lock_guard<std::mutex> lock(_queueMutex);
                return !_queue.empty();
            }

            void popMessage()
            {
                std::lock_guard<std::mutex> lock(_queueMutex);
                _queue.pop();
            }

        private:
            WebSocket _webSocket;

            std::queue<std::string> _queue;
            std::mutex _queueMutex;
    };

    LuaWebSocket* WebSocket_new(lua_State* /*L*/)
    {
        LuaWebSocket* webSocket = new LuaWebSocket();
        return webSocket;
    }

    int WebSocket_getUrl(lua_State* L)
    {
        LuaWebSocket* luaWebSocket = luaW_check<LuaWebSocket>(L, 1);
        lua_pushstring(L, luaWebSocket->getUrl().c_str());
        return 1;
    }

    int WebSocket_setUrl(lua_State* L)
    {
        LuaWebSocket* luaWebSocket = luaW_check<LuaWebSocket>(L, 1);
        const char* url = luaL_checkstring(L, 2);
        luaWebSocket->setUrl(url);
        return 0;
    }

    int WebSocket_start(lua_State* L)
    {
        LuaWebSocket* luaWebSocket = luaW_check<LuaWebSocket>(L, 1);
        luaWebSocket->start();
        return 0;
    }

    int WebSocket_send(lua_State* L)
    {
        LuaWebSocket* luaWebSocket = luaW_check<LuaWebSocket>(L, 1);
        const char* msg = luaL_checkstring(L, 2);
        luaWebSocket->send(msg);
        return 0;
    }

    int WebSocket_getMessage(lua_State* L)
    {
        LuaWebSocket* luaWebSocket = luaW_check<LuaWebSocket>(L, 1);
        lua_pushstring(L, luaWebSocket->getMessage().c_str());
        return 1;
    }

    int WebSocket_hasMessage(lua_State* L)
    {
        LuaWebSocket* luaWebSocket = luaW_check<LuaWebSocket>(L, 1);
        lua_pushboolean(L, luaWebSocket->hasMessage());
        return 1;
    }

    int WebSocket_popMessage(lua_State* L)
    {
        LuaWebSocket* luaWebSocket = luaW_check<LuaWebSocket>(L, 1);
        luaWebSocket->popMessage();
        return 1;
    }

    // FIXME: This should be a static method, or be part of a different module
    int WebSocket_sleep(lua_State* L)
    {
        // LuaWebSocket* luaWebSocket = luaW_check<LuaWebSocket>(L, 1);
        auto duration = luaL_checkinteger(L, 2);
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
        { "getMessage", WebSocket_getMessage },
        { "hasMessage", WebSocket_hasMessage },
        { "popMessage", WebSocket_popMessage },
        { "sleep", WebSocket_sleep },
        { NULL, NULL }
    };

    int luaopen_WebSocket(lua_State* L)
    {
        luaW_register<LuaWebSocket>(L,
            "WebSocket",
            WebSocket_table,
            WebSocket_metatable,
            WebSocket_new
            );
        return 1;
    }
}
