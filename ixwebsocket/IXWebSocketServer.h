/*
 *  IXWebSocketServer.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <utility> // pair
#include <string>
#include <vector>
#include <thread>

namespace ix 
{
    class WebSocketServer {
    public:
        WebSocketServer(int port = 8080);
        virtual ~WebSocketServer();

        std::pair<bool, std::string> run();

        void handleConnection(int fd);

    private:
        int _port;

        std::vector<std::thread> _workers;
    };
}
