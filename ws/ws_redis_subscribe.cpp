/*
 *  ws_redis_subscribe.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <atomic>
#include <chrono>
#include <iostream>
#include <ixsnake/IXRedisClient.h>
#include <sstream>
#include <thread>

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

        std::atomic<int> msgPerSeconds(0);
        std::atomic<int> msgCount(0);

        auto callback = [&msgPerSeconds, &msgCount, verbose](const std::string& message) {
            if (verbose)
            {
                std::cout << "received: " << message << std::endl;
            }

            msgPerSeconds++;
            msgCount++;
        };

        auto responseCallback = [](const std::string& redisResponse) {
            std::cout << "Redis subscribe response: " << redisResponse << std::endl;
        };

        auto timer = [&msgPerSeconds, &msgCount] {
            while (true)
            {
                std::cout << "#messages " << msgCount << " "
                          << "msg/s " << msgPerSeconds << std::endl;

                msgPerSeconds = 0;
                auto duration = std::chrono::seconds(1);
                std::this_thread::sleep_for(duration);
            }
        };

        std::thread t(timer);

        std::cerr << "Subscribing to " << channel << "..." << std::endl;
        if (!redisClient.subscribe(channel, responseCallback, callback))
        {
            std::cerr << "Error subscribing to channel " << channel << std::endl;
            return 1;
        }

        return 0;
    }
} // namespace ix
