/*
 *  IXCobraEvent.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXCobraEventType.h"
#include <cstdint>
#include <ixwebsocket/IXWebSocketHttpHeaders.h>
#include <memory>
#include <string>

namespace ix
{
    struct CobraEvent
    {
        ix::CobraEventType type;
        const std::string& errMsg;
        const ix::WebSocketHttpHeaders& headers;
        const std::string& subscriptionId;
        uint64_t msgId; // CobraConnection::MsgId

        CobraEvent(ix::CobraEventType t,
                   const std::string& e,
                   const ix::WebSocketHttpHeaders& h,
                   const std::string& s,
                   uint64_t m)
            : type(t)
            , errMsg(e)
            , headers(h)
            , subscriptionId(s)
            , msgId(m)
        {
            ;
        }
    };

    using CobraEventPtr = std::unique_ptr<CobraEvent>;
} // namespace ix
