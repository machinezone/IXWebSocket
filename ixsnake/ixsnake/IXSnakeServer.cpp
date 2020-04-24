/*
 *  IXSnakeServer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXSnakeServer.h"

#include "IXAppConfig.h"
#include "IXSnakeConnectionState.h"
#include "IXSnakeProtocol.h"
#include <iostream>
#include <ixcore/utils/IXCoreLogger.h>
#include <sstream>


namespace snake
{
    SnakeServer::SnakeServer(const AppConfig& appConfig)
        : _appConfig(appConfig)
        , _server(appConfig.port, appConfig.hostname)
    {
        _server.setTLSOptions(appConfig.socketTLSOptions);

        if (appConfig.disablePong)
        {
            _server.disablePong();
        }

        std::stringstream ss;
        ss << "Listening on " << appConfig.hostname << ":" << appConfig.port;
        ix::CoreLogger::log(ss.str().c_str());
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
            std::string appkey = path.substr(idx + 1);
            return appkey;
        }
        else
        {
            return std::string();
        }
    }

    bool SnakeServer::run()
    {
        auto factory = []() -> std::shared_ptr<ix::ConnectionState> {
            return std::make_shared<SnakeConnectionState>();
        };
        _server.setConnectionStateFactory(factory);

        _server.setOnConnectionCallback(
            [this](std::shared_ptr<ix::WebSocket> webSocket,
                   std::shared_ptr<ix::ConnectionState> connectionState) {
                auto state = std::dynamic_pointer_cast<SnakeConnectionState>(connectionState);

                webSocket->setOnMessageCallback(
                    [this, webSocket, state](const ix::WebSocketMessagePtr& msg) {
                        std::stringstream ss;
                        ix::LogLevel logLevel = ix::LogLevel::Debug;
                        if (msg->type == ix::WebSocketMessageType::Open)
                        {
                            ss << "New connection" << std::endl;
                            ss << "id: " << state->getId() << std::endl;
                            ss << "Uri: " << msg->openInfo.uri << std::endl;
                            ss << "Headers:" << std::endl;
                            for (auto it : msg->openInfo.headers)
                            {
                                ss << it.first << ": " << it.second << std::endl;
                            }

                            std::string appkey = parseAppKey(msg->openInfo.uri);
                            state->setAppkey(appkey);

                            // Connect to redis first
                            if (!state->redisClient().connect(_appConfig.redisHosts[0],
                                                              _appConfig.redisPort))
                            {
                                ss << "Cannot connect to redis host" << std::endl;
                                logLevel = ix::LogLevel::Error;
                            }
                        }
                        else if (msg->type == ix::WebSocketMessageType::Close)
                        {
                            ss << "Closed connection"
                               << " code " << msg->closeInfo.code << " reason "
                               << msg->closeInfo.reason << std::endl;
                        }
                        else if (msg->type == ix::WebSocketMessageType::Error)
                        {
                            std::stringstream ss;
                            ss << "Connection error: " << msg->errorInfo.reason << std::endl;
                            ss << "#retries: " << msg->errorInfo.retries << std::endl;
                            ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                            ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                            logLevel = ix::LogLevel::Error;
                        }
                        else if (msg->type == ix::WebSocketMessageType::Fragment)
                        {
                            ss << "Received message fragment" << std::endl;
                        }
                        else if (msg->type == ix::WebSocketMessageType::Message)
                        {
                            ss << "Received " << msg->wireSize << " bytes" << std::endl;
                            processCobraMessage(state, webSocket, _appConfig, msg->str);
                        }

                        ix::CoreLogger::log(ss.str().c_str(), logLevel);
                    });
            });

        auto res = _server.listen();
        if (!res.first)
        {
            std::cerr << res.second << std::endl;
            return false;
        }

        _server.start();
        return true;
    }

    void SnakeServer::runForever()
    {
        if (run())
        {
            _server.wait();
        }
    }

    void SnakeServer::stop()
    {
        _server.stop();
    }
} // namespace snake
