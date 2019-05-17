/*
 *  IXSnakeServer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <IXSnakeServer.h>
#include <IXSnakeProtocol.h>
#include <IXSnakeConnectionState.h>
#include <IXAppConfig.h>

#include <iostream>
#include <sstream>

namespace snake
{
    SnakeServer::SnakeServer(const AppConfig& appConfig) :
        _appConfig(appConfig),
        _server(appConfig.port, appConfig.hostname)
    {
        ;
    }

    //
    // Parse appkey from this uri. Won't work if multiple args are present in the uri
    // Uri: /v2?appkey=FC2F10139A2BAc53BB72D9db967b024f
    //
    std::string SnakeServer::parseAppKey(const std::string& path)
    {
        std::string::size_type idx;

        idx = path.rfind('=');
        if (idx != std::string::npos)
        {
            std::string appkey = path.substr(idx+1);
            return appkey;
        }
        else
        {
            return std::string();
        }
    }

    bool SnakeServer::run()
    {
        std::cout << "Listening on " << _appConfig.hostname << ":" << _appConfig.port << std::endl;

        auto factory = []() -> std::shared_ptr<ix::ConnectionState>
        {
            return std::make_shared<SnakeConnectionState>();
        };
        _server.setConnectionStateFactory(factory);

        _server.setOnConnectionCallback(
            [this](std::shared_ptr<ix::WebSocket> webSocket,
                   std::shared_ptr<ix::ConnectionState> connectionState)
            {
                auto state = std::dynamic_pointer_cast<SnakeConnectionState>(connectionState);

                webSocket->setOnMessageCallback(
                    [this, webSocket, state](ix::WebSocketMessageType messageType,
                       const std::string& str,
                       size_t wireSize,
                       const ix::WebSocketErrorInfo& error,
                       const ix::WebSocketOpenInfo& openInfo,
                       const ix::WebSocketCloseInfo& closeInfo)
                    {
                        if (messageType == ix::WebSocketMessageType::Open)
                        {
                            std::cerr << "New connection" << std::endl;
                            std::cerr << "id: " << state->getId() << std::endl;
                            std::cerr << "Uri: " << openInfo.uri << std::endl;
                            std::cerr << "Headers:" << std::endl;
                            for (auto it : openInfo.headers)
                            {
                                std::cerr << it.first << ": " << it.second << std::endl;
                            }

                            std::string appkey = parseAppKey(openInfo.uri);
                            state->setAppkey(appkey);

                            // Connect to redis first
                            if (!state->redisClient().connect(_appConfig.redisHosts[0],
                                                              _appConfig.redisPort))
                            {
                                std::cerr << "Cannot connect to redis host" << std::endl;
                            }
                        }
                        else if (messageType == ix::WebSocketMessageType::Close)
                        {
                            std::cerr << "Closed connection"
                                      << " code " << closeInfo.code
                                      << " reason " << closeInfo.reason << std::endl;
                        }
                        else if (messageType == ix::WebSocketMessageType::Error)
                        {
                            std::stringstream ss;
                            ss << "Connection error: " << error.reason      << std::endl;
                            ss << "#retries: "         << error.retries     << std::endl;
                            ss << "Wait time(ms): "    << error.wait_time   << std::endl;
                            ss << "HTTP Status: "      << error.http_status << std::endl;
                            std::cerr << ss.str();
                        }
                        else if (messageType == ix::WebSocketMessageType::Fragment)
                        {
                            std::cerr << "Received message fragment" << std::endl;
                        }
                        else if (messageType == ix::WebSocketMessageType::Message)
                        {
                            std::cerr << "Received " << wireSize << " bytes" << std::endl;
                            processCobraMessage(state, webSocket, _appConfig, str);
                        }
                    }
                );
            }
        );

        auto res = _server.listen();
        if (!res.first)
        {
            std::cerr << res.second << std::endl;
            return false;
        }

        _server.start();
        _server.wait();

        return true;
    }
}
