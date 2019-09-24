/*
 *  ws_redis_publish.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <ixsnake/IXRedisServer.h>
#include <spdlog/spdlog.h>
#include <sstream>

namespace ix
{
    int ws_redis_server_main(int port, const std::string& hostname)
    {
        spdlog::info("Listening on {}:{}", hostname, port);

        ix::RedisServer server(port, hostname);

        auto res = server.listen();
        if (!res.first)
        {
            std::cerr << res.second << std::endl;
            return 1;
        }

        server.start();
        server.wait();

        return 0;
    }
} // namespace ix
