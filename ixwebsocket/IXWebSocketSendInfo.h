/*
 *  IXWebSocketSendInfo.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <string>

namespace ix 
{
    struct WebSocketSendInfo
    {
        bool success;
        size_t payloadSize;
        size_t wireSize;

        WebSocketSendInfo(bool s = false, size_t p = -1, size_t w = -1)
        {
            success = s;
            payloadSize = p;
            wireSize = w;
        }
    };
}
