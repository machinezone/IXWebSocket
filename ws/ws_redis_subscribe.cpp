/*
 *  ws_redis_subscribe.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <atomic>
#include <chrono>
#include <ixsnake/IXRedisClient.h>
#include <spdlog/spdlog.h>
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

        std::atomic<int> msgPerSeconds(0);
        std::atomic<int> msgCount(0);

        auto callback = [&msgPerSeconds, &msgCount, verbose](const std::string& message) {
            if (verbose)
            {
                spdlog::info("recived: {}", message);
            }

            msgPerSeconds++;
            msgCount++;
        };

        auto responseCallback = [](const std::string& redisResponse) {
            spdlog::info("Redis subscribe response: {}", redisResponse);
        };

        auto timer = [&msgPerSeconds, &msgCount] {
            while (true)
            {
                spdlog::info("#messages {} msg/s {}", msgCount, msgPerSeconds);

                msgPerSeconds = 0;
                auto duration = std::chrono::seconds(1);
                std::this_thread::sleep_for(duration);
            }
        };

        std::thread t(timer);

        spdlog::info("Subscribing to {} ...", channel);
        if (!redisClient.subscribe(channel, responseCallback, callback))
        {
            spdlog::info("Error subscribing to channel {}", channel);
            return 1;
        }

        return 0;
    }
} // namespace ix
