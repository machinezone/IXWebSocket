/*
 *  IXSnakeConnectionState.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <ixredis/IXRedisClient.h>
#include <future>
#include <ixwebsocket/IXConnectionState.h>
#include <string>

namespace snake
{
    class SnakeConnectionState : public ix::ConnectionState
    {
    public:
        std::string getNonce()
        {
            return _nonce;
        }

        void setNonce(const std::string& nonce)
        {
            _nonce = nonce;
        }

        std::string appkey()
        {
            return _appkey;
        }
        void setAppkey(const std::string& appkey)
        {
            _appkey = appkey;
        }

        std::string role()
        {
            return _role;
        }
        void setRole(const std::string& role)
        {
            _role = role;
        }

        ix::RedisClient& redisClient()
        {
            return _redisClient;
        }

        std::future<void> fut;

    private:
        std::string _nonce;
        std::string _role;
        std::string _appkey;

        ix::RedisClient _redisClient;
    };
} // namespace snake
