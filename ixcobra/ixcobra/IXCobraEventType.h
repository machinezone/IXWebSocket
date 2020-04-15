/*
 *  IXCobraEventType.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#pragma once

namespace ix
{
    enum class CobraEventType
    {
        Authenticated = 0,
        Error = 1,
        Open = 2,
        Closed = 3,
        Subscribed = 4,
        UnSubscribed = 5,
        Published = 6,
        Pong = 7,
        HandshakeError = 8,
        AuthenticationError = 9,
        SubscriptionError = 10
    };
}
