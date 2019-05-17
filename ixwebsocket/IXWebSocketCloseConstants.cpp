/*
 *  IXWebSocketCloseConstants.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocketCloseConstants.h"

namespace ix
{
    const uint16_t WebSocketCloseConstants::kNormalClosureCode(1000);
    const uint16_t WebSocketCloseConstants::kInternalErrorCode(1011);
    const uint16_t WebSocketCloseConstants::kAbnormalCloseCode(1006);
    const uint16_t WebSocketCloseConstants::kProtocolErrorCode(1002);
    const uint16_t WebSocketCloseConstants::kNoStatusCodeErrorCode(1005);

    const std::string WebSocketCloseConstants::kNormalClosureMessage("Normal closure");
    const std::string WebSocketCloseConstants::kInternalErrorMessage("Internal error");
    const std::string WebSocketCloseConstants::kAbnormalCloseMessage("Abnormal closure");
    const std::string WebSocketCloseConstants::kPingTimeoutMessage("Ping timeout");
    const std::string WebSocketCloseConstants::kProtocolErrorMessage("Protocol error");
    const std::string WebSocketCloseConstants::kNoStatusCodeErrorMessage("No status code");
}
