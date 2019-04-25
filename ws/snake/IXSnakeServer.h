/*
 *  IXSnakeServer.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <string>

#include <ixwebsocket/IXWebSocketServer.h>
#include "IXAppConfig.h"

namespace snake
{
    class SnakeServer
    {
    public:
        SnakeServer(const AppConfig& appConfig);
        ~SnakeServer() = default;

        bool run();

    private:
        std::string parseAppKey(const std::string& path);

        AppConfig _appConfig;
        ix::WebSocketServer _server;
    };
}
