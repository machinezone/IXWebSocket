/*
 *  IXSocketServer.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXConnectionState.h"

#include <utility> // pair
#include <string>
#include <set>
#include <thread>
#include <list>
#include <mutex>
#include <functional>
#include <memory>
#include <atomic>
#include <condition_variable>

namespace ix
{
    class SocketServer {
    public:
        using ConnectionStateFactory = std::function<std::shared_ptr<ConnectionState>()>;

        // Each connection is handled by its own worker thread.
        // We use a list as we only care about remove and append operations.
        using ConnectionThreads = std::list<std::pair<std::shared_ptr<ConnectionState>,
                                                      std::thread>>;

        SocketServer(int port = SocketServer::kDefaultPort,
                     const std::string& host = SocketServer::kDefaultHost,
                     int backlog = SocketServer::kDefaultTcpBacklog,
                     size_t maxConnections = SocketServer::kDefaultMaxConnections);
        virtual ~SocketServer();
        virtual void stop();

        // It is possible to override ConnectionState through inheritance
        // this method allows user to change the factory by returning an object
        // that inherits from ConnectionState but has its own methods.
        void setConnectionStateFactory(const ConnectionStateFactory& connectionStateFactory);

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

        void stopAcceptingConnections();

    private:
        // Member variables
        int _port;
        std::string _host;
        int _backlog;
        size_t _maxConnections;

        // socket for accepting connections
        int _serverFd;

        std::mutex _logMutex;

        // background thread to wait for incoming connections
        std::atomic<bool> _stop;
        std::thread _thread;

        // the list of (connectionState, threads) for each connections
        ConnectionThreads _connectionsThreads;
        std::mutex _connectionsThreadsMutex;

        // used to have the main control thread for a server
        // wait for a 'terminate' notification without busy polling
        std::condition_variable _conditionVariable;
        std::mutex _conditionVariableMutex;

        // the factory to create ConnectionState objects
        ConnectionStateFactory _connectionStateFactory;

        // Methods
        void run();
        virtual void handleConnection(int fd,
                                      std::shared_ptr<ConnectionState> connectionState) = 0;
        virtual size_t getConnectedClientsCount() = 0;

        // Returns true if all connection threads are joined
        bool closeTerminatedThreads();
    };
}
