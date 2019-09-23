/*
 *  IXSnakeProtocol.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <memory>
#include <string>

namespace ix
{
    class WebSocket;
}

namespace snake
{
    class SnakeConnectionState;
    struct AppConfig;

    void processCobraMessage(std::shared_ptr<SnakeConnectionState> state,
                             std::shared_ptr<ix::WebSocket> ws,
                             const AppConfig& appConfig,
                             const std::string& str);
} // namespace snake
