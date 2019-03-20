/*
 *  IXRedisClient.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <memory>
#include <functional>

namespace ix
{
    class Socket;

    class RedisClient {
    public:
        using OnRedisSubscribeCallback = std::function<void(const std::string&)>;

        RedisClient() = default;
        ~RedisClient() = default;

        bool connect(const std::string& hostname,
                     int port);

        bool publish(const std::string& channel,
                     const std::string& message);

        bool subscribe(const std::string& channel,
                       const OnRedisSubscribeCallback& callback);

    private:
        std::shared_ptr<Socket> _socket;
    };
}

