/*
 *  IXWebSocketErrorInfo.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXWebSocketHttpHeaders.h"
#include <cstdint>
#include <string>

namespace ix
{
    struct WebSocketErrorInfo
    {
        uint32_t retries = 0;
        double wait_time = 0;
        int http_status = 0;
        std::string reason;
        bool decompressionError = false;
        // HTTP response headers available when the server rejects the handshake.
        WebSocketHttpHeaders headers;
    };
} // namespace ix
