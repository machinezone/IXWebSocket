/*
 *  ws_redis_subscribe.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include "IXRedisClient.h"

namespace ix
{
    int ws_redis_subscribe_main(const std::string& hostname,
                                int port,
                                const std::string& channel)
    {
        RedisClient redisClient;
        if (!redisClient.connect(hostname, port))
        {
            std::cerr << "Cannot connect to redis host" << std::endl;
            return 1;
        }

        return 0;
    }
}

