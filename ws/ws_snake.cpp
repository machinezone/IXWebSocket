/*
 *  snake_run.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXSnakeServer.h"

#include <iostream>
#include <sstream>
#include <fstream>

namespace
{
    std::vector<uint8_t> load(const std::string& path)
    {
        std::vector<uint8_t> memblock;

        std::ifstream file(path);
        if (!file.is_open()) return memblock;

        file.seekg(0, file.end);
        std::streamoff size = file.tellg();
        file.seekg(0, file.beg);

        memblock.resize(size);
        file.read((char*)&memblock.front(), static_cast<std::streamsize>(size));

        return memblock;
    }

    std::string readAsString(const std::string& path)
    {
        auto vec = load(path);
        return std::string(vec.begin(), vec.end());
    }
}

namespace ix
{
    int ws_snake_main(int port,
                      const std::string& hostname,
                      const std::string& redisHosts,
                      int redisPort,
                      const std::string& redisPassword,
                      bool verbose,
                      const std::string& appsConfigPath)
    {
        snake::AppConfig appConfig;
        appConfig.port = port;
        appConfig.hostname = hostname;
        appConfig.verbose = verbose;
        appConfig.redisPort = redisPort;
        appConfig.redisPassword = redisPassword;

        // Parse config file
        auto str = readAsString(appsConfigPath);
        if (str.empty())
        {
            std::cout << "Cannot read content of " << appsConfigPath << std::endl;
            return 1;
        }

        std::cout << str << std::endl;
        auto apps = nlohmann::json::parse(str);
        appConfig.apps = apps["apps"];

        std::string token;
        std::stringstream tokenStream(redisHosts);
        while (std::getline(tokenStream, token, ';'))
        {
            appConfig.redisHosts.push_back(token);
        }

        // Display config on the terminal for debugging
        dumpConfig(appConfig);

        snake::SnakeServer snakeServer(appConfig);
        return snakeServer.run() ? 0 : 1;
    }
}
