/*
 *  IXRedisClient.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <ixwebsocket/IXSocket.h>

namespace ix
{
    class RedisClient {
    public:
        RedisClient();
        ~RedisClient();

        bool connect(const std::string& hostname, int port);

    private:
        std::shared_ptr<Socket> _socket;
    };
}

