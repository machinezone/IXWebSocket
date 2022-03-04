/*
 *  IXWebSocketCloseConstants.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXWebsocketExport.h"
#include <cstdint>
#include <string>

namespace ix
{
    struct WebSocketCloseConstants
    {
        IXWEBSOCKET_EXPORT static const uint16_t kNormalClosureCode;
        IXWEBSOCKET_EXPORT static const uint16_t kInternalErrorCode;
        IXWEBSOCKET_EXPORT static const uint16_t kAbnormalCloseCode;
        IXWEBSOCKET_EXPORT static const uint16_t kProtocolErrorCode;
        IXWEBSOCKET_EXPORT static const uint16_t kNoStatusCodeErrorCode;
        IXWEBSOCKET_EXPORT static const uint16_t kInvalidFramePayloadData;

        IXWEBSOCKET_EXPORT static const std::string kNormalClosureMessage;
        IXWEBSOCKET_EXPORT static const std::string kInternalErrorMessage;
        IXWEBSOCKET_EXPORT static const std::string kAbnormalCloseMessage;
        IXWEBSOCKET_EXPORT static const std::string kPingTimeoutMessage;
        IXWEBSOCKET_EXPORT static const std::string kProtocolErrorMessage;
        IXWEBSOCKET_EXPORT static const std::string kNoStatusCodeErrorMessage;
        IXWEBSOCKET_EXPORT static const std::string kProtocolErrorReservedBitUsed;
        IXWEBSOCKET_EXPORT static const std::string kProtocolErrorPingPayloadOversized;
        IXWEBSOCKET_EXPORT static const std::string kProtocolErrorCodeControlMessageFragmented;
        IXWEBSOCKET_EXPORT static const std::string kProtocolErrorCodeDataOpcodeOutOfSequence;
        IXWEBSOCKET_EXPORT static const std::string kProtocolErrorCodeContinuationOpCodeOutOfSequence;
        IXWEBSOCKET_EXPORT static const std::string kInvalidFramePayloadDataMessage;
        IXWEBSOCKET_EXPORT static const std::string kInvalidCloseCodeMessage;
    };
} // namespace ix
