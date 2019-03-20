/*
 *  ws_redis_publish.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include "IXRedisClient.h"

namespace ix
{
    int ws_redis_publish_main(const std::string& hostname,
                              int port,
                              const std::string& channel,
                              const std::string& message)
    {
        RedisClient redisClient;
        if (!redisClient.connect(hostname, port))
        {
            std::cerr << "Cannot connect to redis host" << std::endl;
            return 1;
        }

        std::cerr << "Publishing message " << message
                  << " to " << channel << "..." << std::endl;
        if (!redisClient.publish(channel, message))
        {
            std::cerr << "Error publishing message to redis" << std::endl;
            return 1;
        }

        return 0;
    }
}
