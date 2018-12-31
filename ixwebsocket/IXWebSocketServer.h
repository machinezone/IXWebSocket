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
#include <functional>

#include "IXWebSocket.h"

namespace ix 
{
    using OnConnectionCallback = std::function<void(WebSocket&)>;

    class WebSocketServer {
    public:
        WebSocketServer(int port = 8080, int backlog = 5);
        virtual ~WebSocketServer();

        void setOnConnectionCallback(const OnConnectionCallback& callback);

        std::pair<bool, std::string> listen();
        void run();

    private:
        void handleConnection(int fd);

        int _port;
        int _backlog;

        OnConnectionCallback _onConnectionCallback;

        // socket for accepting connections
        int _serverFd;

        // FIXME: we never reclaim space in this array ...
        std::vector<std::thread> _workers;
    };
}
