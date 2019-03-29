/*
 *  IXRedisClient.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <memory>
#include <functional>
#include <cpp_redis/cpp_redis>

namespace ix
{
    class Socket;

    class RedisClient {
    public:
        using OnRedisSubscribeResponseCallback = std::function<void(const std::string&)>;
        using OnRedisSubscribeCallback = std::function<void(const std::string&)>;

        RedisClient() = default;
        ~RedisClient() = default;

        bool connect(const std::string& hostname,
                     int port);

        bool auth(const std::string& password,
                  std::string& response);

        bool publish(const std::string& channel,
                     const std::string& message);

        bool subscribe(const std::string& channel,
                       const OnRedisSubscribeResponseCallback& responseCallback,
                       const OnRedisSubscribeCallback& callback);

    private:
        cpp_redis::subscriber _sub;

        std::shared_ptr<Socket> _socket;
    };
}

