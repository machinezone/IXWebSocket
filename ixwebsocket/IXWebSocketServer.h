/*
 *  IXWebSocketServer.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <utility> // pair
#include <string>

namespace ix 
{
    class WebSocketServer {
    public:
        WebSocketServer(int port = 8080);
        virtual ~WebSocketServer();

        std::pair<bool, std::string> run();

    private:
        int _port;
    };
}
