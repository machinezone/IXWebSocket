/*
 *  IXWebSocketCloseConstants.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <cstdint>
#include <string>

namespace ix
{
    struct WebSocketCloseConstants
    {
        static const uint16_t kNormalClosureCode;
        static const uint16_t kInternalErrorCode;
        static const uint16_t kAbnormalCloseCode;
        static const uint16_t kProtocolErrorCode;
        static const uint16_t kNoStatusCodeErrorCode;

        static const std::string kNormalClosureMessage;
        static const std::string kInternalErrorMessage;
        static const std::string kAbnormalCloseMessage;
        static const std::string kPingTimeoutMessage;
        static const std::string kProtocolErrorMessage;
        static const std::string kNoStatusCodeErrorMessage;
    };
}
