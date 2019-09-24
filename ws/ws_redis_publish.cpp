/*
 *  ws_redis_publish.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <ixsnake/IXRedisClient.h>
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
            std::cerr << "Cannot connect to redis host" << std::endl;
            return 1;
        }

        if (!password.empty())
        {
            std::string authResponse;
            if (!redisClient.auth(password, authResponse))
            {
                std::stringstream ss;
                std::cerr << "Cannot authenticated to redis" << std::endl;
                return 1;
            }
            std::cout << "Auth response: " << authResponse << ":" << port << std::endl;
        }

        std::string errMsg;
        for (int i = 0; i < count; i++)
        {
            if (!redisClient.publish(channel, message, errMsg))
            {
                std::cerr << "Error publishing to channel " << channel << "error: " << errMsg
                          << std::endl;
                return 1;
            }
        }

        return 0;
    }
} // namespace ix
