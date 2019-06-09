/*
 *  IXWebSocketMessage.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXWebSocketMessageType.h"
#include "IXWebSocketErrorInfo.h"
#include "IXWebSocketOpenInfo.h"
#include "IXWebSocketCloseInfo.h"
#include <string>
#include <memory>
#include <thread>

namespace ix
{
    struct WebSocketMessage
    {
        WebSocketMessageType type;
        std::string str;
        size_t wireSize;
        WebSocketErrorInfo errorInfo;
        WebSocketOpenInfo openInfo;
        WebSocketCloseInfo closeInfo;
        bool binary;
    };

    using WebSocketMessagePtr = std::shared_ptr<WebSocketMessage>;
} // namespace ix
