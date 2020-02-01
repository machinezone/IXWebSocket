/*
 *  snake_run.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include <fstream>
#include <ixsnake/IXSnakeServer.h>
#include <spdlog/spdlog.h>
#include <sstream>

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

        memblock.resize((size_t) size);
        file.read((char*) &memblock.front(), static_cast<std::streamsize>(size));

        return memblock;
    }

    std::string readAsString(const std::string& path)
    {
        auto vec = load(path);
        return std::string(vec.begin(), vec.end());
    }
} // namespace

namespace ix
{
    int ws_snake_main(int port,
                      const std::string& hostname,
                      const std::string& redisHosts,
                      int redisPort,
                      const std::string& redisPassword,
                      bool verbose,
                      const std::string& appsConfigPath,
                      const SocketTLSOptions& socketTLSOptions,
                      bool disablePong)
    {
        snake::AppConfig appConfig;
        appConfig.port = port;
        appConfig.hostname = hostname;
        appConfig.verbose = verbose;
        appConfig.redisPort = redisPort;
        appConfig.redisPassword = redisPassword;
        appConfig.socketTLSOptions = socketTLSOptions;
        appConfig.disablePong = disablePong;

        // Parse config file
        auto str = readAsString(appsConfigPath);
        if (str.empty())
        {
            spdlog::error("Cannot read content of {}", appsConfigPath);
            return 1;
        }

        spdlog::error(str);
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
        snakeServer.runForever();

        return 0; // should never reach this
    }
} // namespace ix
