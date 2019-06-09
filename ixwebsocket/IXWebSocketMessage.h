/*
 *  IXWebSocketMessage.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXWebSocketCloseInfo.h"
#include "IXWebSocketErrorInfo.h"
#include "IXWebSocketMessageType.h"
#include "IXWebSocketOpenInfo.h"
#include <memory>
#include <string>
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

        WebSocketMessage(WebSocketMessageType t,
                         const std::string& s,
                         size_t w,
                         WebSocketErrorInfo e,
                         WebSocketOpenInfo o,
                         WebSocketCloseInfo c,
                         bool b = false)
            : type(t)
            , str(std::move(s))
            , wireSize(w)
            , errorInfo(e)
            , openInfo(o)
            , closeInfo(c)
            , binary(b)
        {
            ;
        }
    };

    using WebSocketMessagePtr = std::shared_ptr<WebSocketMessage>;
} // namespace ix
