/*
 *  IXWebSocketOpenInfo.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

namespace ix
{
    struct WebSocketOpenInfo
    {
        std::string uri;
        WebSocketHttpHeaders headers;

        WebSocketOpenInfo(const std::string& u = std::string(),
                          const WebSocketHttpHeaders& h = WebSocketHttpHeaders())
            : uri(u)
            , headers(h)
        {
            ;
        }
    };
} // namespace ix
