/*
 *  IXRedisClient.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXRedisClient.h"

#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>

namespace ix
{
    RedisClient::RedisClient()
    {

    }

    RedisClient::~RedisClient()
    {

    }

    bool RedisClient::connect(const std::string& hostname, int port)
    {
        return true;
    }
}
