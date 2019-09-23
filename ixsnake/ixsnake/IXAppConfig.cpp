/*
 *  IXSnakeProtocol.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXAppConfig.h"

#include "IXSnakeProtocol.h"
#include <iostream>
#include <ixcrypto/IXUuid.h>

namespace snake
{
    bool isAppKeyValid(const AppConfig& appConfig, std::string appkey)
    {
        return appConfig.apps.count(appkey) != 0;
    }

    std::string getRoleSecret(const AppConfig& appConfig, std::string appkey, std::string role)
    {
        if (!isAppKeyValid(appConfig, appkey))
        {
            std::cerr << "Missing appkey " << appkey << std::endl;
            return std::string();
        }

        auto roles = appConfig.apps[appkey]["roles"];
        auto channel = roles[role]["secret"];
        return channel;
    }

    std::string generateNonce()
    {
        return ix::uuid4();
    }

    void dumpConfig(const AppConfig& appConfig)
    {
        for (auto&& host : appConfig.redisHosts)
        {
            std::cout << "redis host: " << host << std::endl;
        }

        std::cout << "redis password: " << appConfig.redisPassword << std::endl;
        std::cout << "redis port: " << appConfig.redisPort << std::endl;
    }
} // namespace snake
