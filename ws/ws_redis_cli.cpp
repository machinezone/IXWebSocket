/*
 *  ws_redis_cli.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <ixredis/IXRedisClient.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <iostream>

namespace ix
{
    int ws_redis_cli_main(const std::string& hostname,
                          int port,
                          const std::string& password)
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

        while (true)
        {
            std::string text;
            std::cout << "> " << std::flush;
            std::getline(std::cin, text);

#if 0
            if (!redisClient.send(args, errMsg))
            {
                spdlog::error("Error", channel, errMsg);
                return 1;
            }
#endif
        }

        return 0;
    }
} // namespace ix
