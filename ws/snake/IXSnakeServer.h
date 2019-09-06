/*
 *  IXSnakeServer.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXAppConfig.h"
#include <ixwebsocket/IXWebSocketServer.h>
#include <string>

namespace snake
{
    class SnakeServer
    {
    public:
        SnakeServer(const AppConfig& appConfig);
        ~SnakeServer() = default;

        bool run();
        void runForever();
        void stop();

    private:
        std::string parseAppKey(const std::string& path);

        AppConfig _appConfig;
        ix::WebSocketServer _server;
    };
} // namespace snake
