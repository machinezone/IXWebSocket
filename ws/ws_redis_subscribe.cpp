/*
 *  ws_redis_subscribe.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <chrono>
#include "IXRedisClient.h"

namespace ix
{
    int ws_redis_subscribe_main(const std::string& hostname,
                                int port,
                                const std::string& password,
                                const std::string& channel,
                                bool verbose)
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

        std::chrono::time_point<std::chrono::steady_clock> lastTimePoint;
        int msgPerSeconds = 0;
        int msgCount = 0;

        auto callback = [&lastTimePoint, &msgPerSeconds, &msgCount, verbose]
                         (const std::string& message)
        {
            if (verbose)
            {
                std::cout << message << std::endl;
            }

            msgPerSeconds++;

            auto now = std::chrono::steady_clock::now();
            if (now - lastTimePoint > std::chrono::seconds(1))
            {
                lastTimePoint = std::chrono::steady_clock::now();

                msgCount += msgPerSeconds;

                // #messages 901 msg/s 150
                std::cout << "#messages " << msgCount << " "
                          << "msg/s " << msgPerSeconds
                          << std::endl;

                msgPerSeconds = 0;
            }
        };

        auto responseCallback = [](const std::string& redisResponse)
        {
            std::cout << "Redis subscribe response: " << redisResponse << std::endl;
        };

        std::cerr << "Subscribing to " << channel << "..." << std::endl;
        if (!redisClient.subscribe(channel, responseCallback, callback))
        {
            std::cerr << "Error subscribing to channel " << channel << std::endl;
            return 1;
        }

        return 0;
    }
}

