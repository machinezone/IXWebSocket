/*
 *  ws_redis_cli.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <ixredis/IXRedisClient.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <iostream>
#include "linenoise.hpp"

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
            // Read line
            std::string line;
            std::string prompt;
            prompt += hostname;
            prompt += ":";
            prompt += std::to_string(port);
            prompt += "> ";
            auto quit = linenoise::Readline(prompt.c_str(), line);

            if (quit)
            {
                break;
            }

            std::stringstream ss(line);
            std::vector<std::string> args;
            std::string arg;

            while (ss.good())
            {
                ss >> arg;
                args.push_back(arg);
            }

            std::string errMsg;
            auto response = redisClient.send(args, errMsg);
            if (!errMsg.empty())
            {
                spdlog::error("(error) {}", errMsg);
            }
            else
            {
                if (response.first != RespType::String)
                {
                    std::cout << "("
                              << redisClient.getRespTypeDescription(response.first)
                              << ")"
                              << " ";
                }

                std::cout << response.second << std::endl;
            }

            linenoise::AddHistory(line.c_str());
        }

        return 0;
    }
} // namespace ix
