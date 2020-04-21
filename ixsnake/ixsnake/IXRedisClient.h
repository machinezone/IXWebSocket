/*
 *  IXRedisClient.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <atomic>
#include <functional>
#include <ixwebsocket/IXSocket.h>
#include <memory>
#include <string>

namespace ix
{
    class RedisClient
    {
    public:
        using OnRedisSubscribeResponseCallback = std::function<void(const std::string&)>;
        using OnRedisSubscribeCallback = std::function<void(const std::string&)>;

        RedisClient()
            : _stop(false)
        {
        }
        ~RedisClient() = default;

        bool connect(const std::string& hostname, int port);

        bool auth(const std::string& password, std::string& response);

        // Publish / Subscribe
        bool publish(const std::string& channel, const std::string& message, std::string& errMsg);

        bool subscribe(const std::string& channel,
                       const OnRedisSubscribeResponseCallback& responseCallback,
                       const OnRedisSubscribeCallback& callback);

        // XADD
        std::string xadd(const std::string& channel,
                         const std::string& message,
                         std::string& errMsg);

        std::string prepareXaddCommand(const std::string& stream, const std::string& message);

        std::string readXaddReply(std::string& errMsg);

        bool sendCommand(const std::string& commands, int commandsCount, std::string& errMsg);

        void stop();

    private:
        std::string writeString(const std::string& str);

        std::unique_ptr<Socket> _socket;
        std::atomic<bool> _stop;
    };
} // namespace ix
