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
#include <condition_variable>

#include "IXWebSocket.h"
#include "IXSocketServer.h"

namespace ix
{
    using OnConnectionCallback = std::function<void(std::shared_ptr<WebSocket>)>;

    class WebSocketServer : public SocketServer {
    public:
        WebSocketServer(int port = SocketServer::kDefaultPort,
                        const std::string& host = SocketServer::kDefaultHost,
                        int backlog = SocketServer::kDefaultTcpBacklog,
                        size_t maxConnections = SocketServer::kDefaultMaxConnections,
                        int handshakeTimeoutSecs = WebSocketServer::kDefaultHandShakeTimeoutSecs);
        virtual ~WebSocketServer();
        virtual void stop() final;

        void setOnConnectionCallback(const OnConnectionCallback& callback);

        // Get all the connected clients
        std::set<std::shared_ptr<WebSocket>> getClients();

    private:
        // Member variables
        int _handshakeTimeoutSecs;

        OnConnectionCallback _onConnectionCallback;

        std::mutex _clientsMutex;
        std::set<std::shared_ptr<WebSocket>> _clients;

        const static int kDefaultHandShakeTimeoutSecs;

        // Methods
        virtual void handleConnection(int fd) final;
        virtual size_t getConnectedClientsCount() final;
    };
}
