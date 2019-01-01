/*
 *  IXWebSocketServer.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <utility> // pair
#include <string>
#include <set>
#include <thread>
#include <mutex>
#include <functional>
#include <memory>

#include "IXWebSocket.h"

namespace ix 
{
    using OnConnectionCallback = std::function<void(std::shared_ptr<WebSocket>)>;

    class WebSocketServer {
    public:
        WebSocketServer(int port = 8080, int backlog = 5);
        virtual ~WebSocketServer();

        void setOnConnectionCallback(const OnConnectionCallback& callback);

        std::pair<bool, std::string> listen();
        void run();

        // FIXME: need mutex
        std::set<std::shared_ptr<WebSocket>> getClients() { return _clients; }

    private:
        void handleConnection(int fd);

        int _port;
        int _backlog;

        OnConnectionCallback _onConnectionCallback;

        // socket for accepting connections
        int _serverFd;

        std::set<std::shared_ptr<WebSocket>> _clients;
    };
}
