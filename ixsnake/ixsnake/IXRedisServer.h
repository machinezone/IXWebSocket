/*
 *  IXRedisServer.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXSocket.h"
#include "IXSocketServer.h"
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility> // pair

namespace ix
{
    class RedisServer final : public SocketServer
    {
    public:
        RedisServer(int port = SocketServer::kDefaultPort,
                    const std::string& host = SocketServer::kDefaultHost,
                    int backlog = SocketServer::kDefaultTcpBacklog,
                    size_t maxConnections = SocketServer::kDefaultMaxConnections,
                    int addressFamily = SocketServer::kDefaultAddressFamily);
        virtual ~RedisServer();
        virtual void stop() final;

    private:
        // Member variables
        std::atomic<int> _connectedClientsCount;

        // Subscribers
        // We could store connection states in there, to add better debugging
        // since a connection state has a readable ID
        std::map<std::string, std::set<Socket*>> _subscribers;
        std::mutex _mutex;

        std::atomic<bool> _stopHandlingConnections;

        // Methods
        virtual void handleConnection(std::unique_ptr<Socket>,
                                      std::shared_ptr<ConnectionState> connectionState) final;
        virtual size_t getConnectedClientsCount() final;

        bool startsWith(const std::string& str, const std::string& start);
        std::string writeString(const std::string& str);

        bool parseRequest(std::unique_ptr<Socket>& socket, std::vector<std::string>& tokens);

        bool handlePublish(std::unique_ptr<Socket>& socket, const std::vector<std::string>& tokens);

        bool handleSubscribe(std::unique_ptr<Socket>& socket,
                             const std::vector<std::string>& tokens);

        bool handleCommand(std::unique_ptr<Socket>& socket, const std::vector<std::string>& tokens);

        void cleanupSubscribers(std::unique_ptr<Socket>& socket);
    };
} // namespace ix
