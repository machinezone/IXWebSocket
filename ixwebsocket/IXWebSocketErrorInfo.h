/*
 *  IXWebSocketErrorInfo.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <string>

namespace ix
{
    struct WebSocketErrorInfo
    {
        uint32_t retries;
        double wait_time;
        int http_status;
        std::string reason;
        bool decompressionError;
    };
}
