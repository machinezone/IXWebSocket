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

namespace ix 
{
    using OnConnectionCallback = std::function<void(std::shared_ptr<WebSocket>)>;

    class WebSocketServer {
    public:
        WebSocketServer(int port = 8080,
                        const std::string& host = WebSocketServer::kDefaultHost,
                        int backlog = 5);
        virtual ~WebSocketServer();

        void setOnConnectionCallback(const OnConnectionCallback& callback);
        void start();
        void wait();

        std::pair<bool, std::string> listen();

        // Get all the connected clients
        std::set<std::shared_ptr<WebSocket>> getClients();

    private:
        // Member variables
        int _port;
        std::string _host;
        int _backlog;

        OnConnectionCallback _onConnectionCallback;

        // socket for accepting connections
        int _serverFd;

        std::mutex _clientsMutex;
        std::set<std::shared_ptr<WebSocket>> _clients;

        std::mutex _logMutex;

        std::atomic<bool> _stop;
        std::thread _thread;

        std::condition_variable _conditionVariable;
        std::mutex _conditionVariableMutex;

        const static std::string kDefaultHost;

        // Methods
        void run();
        void stop();
        void handleConnection(int fd);

        // Logging
        void logError(const std::string& str);
        void logInfo(const std::string& str);
    };
}
