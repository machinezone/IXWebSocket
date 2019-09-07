/*
 *  IXRedisClient.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <functional>
#include <memory>
#include <atomic>

namespace ix
{
    class Socket;

    class RedisClient
    {
    public:
        using OnRedisSubscribeResponseCallback = std::function<void(const std::string&)>;
        using OnRedisSubscribeCallback = std::function<void(const std::string&)>;

        RedisClient() : _stop(false) {}
        ~RedisClient() = default;

        bool connect(const std::string& hostname, int port);

        bool auth(const std::string& password, std::string& response);

        bool publish(const std::string& channel, const std::string& message, std::string& errMsg);

        bool subscribe(const std::string& channel,
                       const OnRedisSubscribeResponseCallback& responseCallback,
                       const OnRedisSubscribeCallback& callback);

        void stop();

    private:
        std::string writeString(const std::string& str);

        std::shared_ptr<Socket> _socket;
        std::atomic<bool> _stop;
    };
} // namespace ix
