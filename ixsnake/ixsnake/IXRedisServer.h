/*
 *  IXRedisServer.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXSocketServer.h"
#include "IXSocket.h"
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <map>
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
                    size_t maxConnections = SocketServer::kDefaultMaxConnections);
        virtual ~RedisServer();
        virtual void stop() final;

    private:
        // Member variables
        std::atomic<int> _connectedClientsCount;

        // Subscribers
        // We could store connection states in there, to add better debugging
        // since a connection state has a readable ID
        std::map<std::string, std::set<std::shared_ptr<Socket>>> _subscribers;
        std::mutex _mutex;

        std::atomic<bool> _stopHandlingConnections;

        // Methods
        virtual void handleConnection(std::shared_ptr<Socket>,
                                      std::shared_ptr<ConnectionState> connectionState) final;
        virtual size_t getConnectedClientsCount() final;

        bool startsWith(const std::string& str, const std::string& start);
        std::string writeString(const std::string& str);

        bool parseRequest(
            std::shared_ptr<Socket> socket,
            std::vector<std::string>& tokens);

        bool handlePublish(std::shared_ptr<Socket> socket,
                           const std::vector<std::string>& tokens);

        bool handleSubscribe(std::shared_ptr<Socket> socket,
                             const std::vector<std::string>& tokens);

        bool handleCommand(std::shared_ptr<Socket> socket,
                           const std::vector<std::string>& tokens);

        void cleanupSubscribers(std::shared_ptr<Socket> socket);
    };
} // namespace ix
