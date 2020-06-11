/*
 *  ws_redis_publish.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <ixredis/IXRedisClient.h>
#include <spdlog/spdlog.h>
#include <sstream>

namespace ix
{
    int ws_redis_publish_main(const std::string& hostname,
                              int port,
                              const std::string& password,
                              const std::string& channel,
                              const std::string& message,
                              int count)
    {
        RedisClient redisClient;
        if (!redisClient.connect(hostname, port))
        {
            spdlog::info("Cannot connect to redis host");
            return 1;
        }

        if (!password.empty())
        {
            std::string authResponse;
            if (!redisClient.auth(password, authResponse))
            {
                std::stringstream ss;
                spdlog::info("Cannot authenticated to redis");
                return 1;
            }
            spdlog::info("Auth response: {}", authResponse);
        }

        std::string errMsg;
        for (int i = 0; i < count; i++)
        {
            if (!redisClient.publish(channel, message, errMsg))
            {
                spdlog::error("Error publishing to channel {} error {}", channel, errMsg);
                return 1;
            }
        }

        return 0;
    }
} // namespace ix
