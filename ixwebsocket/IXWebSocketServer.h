/*
 *  IXWebSocketServer.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXSocketServer.h"
#include "IXWebSocket.h"
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility> // pair

namespace ix
{
    class WebSocketServer : public SocketServer
    {
    public:
        using OnConnectionCallback =
            std::function<void(std::shared_ptr<WebSocket>, std::shared_ptr<ConnectionState>)>;

        WebSocketServer(int port = SocketServer::kDefaultPort,
                        const std::string& host = SocketServer::kDefaultHost,
                        int backlog = SocketServer::kDefaultTcpBacklog,
                        size_t maxConnections = SocketServer::kDefaultMaxConnections,
                        int handshakeTimeoutSecs = WebSocketServer::kDefaultHandShakeTimeoutSecs,
                        int addressFamily = SocketServer::kDefaultAddressFamily);
        virtual ~WebSocketServer();
        virtual void stop() final;

        void enablePong();
        void disablePong();
        void disablePerMessageDeflate();

        void setOnConnectionCallback(const OnConnectionCallback& callback);

        // Get all the connected clients
        std::set<std::shared_ptr<WebSocket>> getClients();

        const static int kDefaultHandShakeTimeoutSecs;

    private:
        // Member variables
        int _handshakeTimeoutSecs;
        bool _enablePong;
        bool _enablePerMessageDeflate;

        OnConnectionCallback _onConnectionCallback;

        std::mutex _clientsMutex;
        std::set<std::shared_ptr<WebSocket>> _clients;

        const static bool kDefaultEnablePong;

        // Methods
        virtual void handleConnection(std::unique_ptr<Socket> socket,
                                      std::shared_ptr<ConnectionState> connectionState) final;
        virtual size_t getConnectedClientsCount() final;
    };
} // namespace ix
