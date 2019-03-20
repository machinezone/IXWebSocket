/*
 *  IXRedisClient.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <memory>

namespace ix
{
    class Socket;

    class RedisClient {
    public:
        RedisClient() = default;
        ~RedisClient() = default;

        bool connect(const std::string& hostname, int port);

        bool publish(const std::string& channel,
                     const std::string& message);

    private:
        std::shared_ptr<Socket> _socket;
    };
}

