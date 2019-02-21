/*
 *  IXSocketServer.h
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
#include <atomic>
#include <condition_variable>

namespace ix
{
    class SocketServer {
    public:
        SocketServer(int port = SocketServer::kDefaultPort,
                     const std::string& host = SocketServer::kDefaultHost,
                     int backlog = SocketServer::kDefaultTcpBacklog,
                     size_t maxConnections = SocketServer::kDefaultMaxConnections);
        virtual ~SocketServer();
        virtual void stop();

        const static int kDefaultPort;
        const static std::string kDefaultHost;
        const static int kDefaultTcpBacklog;
        const static size_t kDefaultMaxConnections;

        void start();
        std::pair<bool, std::string> listen();
        void wait();

    protected:

        // Logging
        void logError(const std::string& str);
        void logInfo(const std::string& str);

    private:
        // Member variables
        int _port;
        std::string _host;
        int _backlog;
        size_t _maxConnections;

        // socket for accepting connections
        int _serverFd;

        std::mutex _logMutex;

        std::atomic<bool> _stop;
        std::thread _thread;

        std::condition_variable _conditionVariable;
        std::mutex _conditionVariableMutex;

        // Methods
        void run();
        virtual void handleConnection(int fd) = 0;
        virtual size_t getConnectedClientsCount() = 0;
    };
}
