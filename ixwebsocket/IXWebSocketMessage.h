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
        std::string& str;
        size_t wireSize;
        WebSocketErrorInfo errorInfo;
        WebSocketOpenInfo openInfo;
        WebSocketCloseInfo closeInfo;
        bool binary;

        static std::string kStr;

        WebSocketMessage(WebSocketMessageType t,
                         size_t w = 0,
                         WebSocketErrorInfo e = WebSocketErrorInfo(),
                         WebSocketOpenInfo o = WebSocketOpenInfo(),
                         WebSocketCloseInfo c = WebSocketCloseInfo(),
                         bool b = false)
            : type(t)
            , str(WebSocketMessage::kStr)
            , wireSize(w)
            , errorInfo(e)
            , openInfo(o)
            , closeInfo(c)
            , binary(b)
        {
            ;
        }

        // void setStr(const std::string& s)
        // {
        //     str = std::move(s);
        // }
    };

    using WebSocketMessagePtr = std::unique_ptr<WebSocketMessage>;
} // namespace ix
