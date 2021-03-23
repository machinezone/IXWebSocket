/*
 *  IXExponentialBackoff.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXExponentialBackoff.h"

#include <cmath>

namespace ix
{
    uint32_t calculateRetryWaitMilliseconds(uint32_t retryCount,
                                            uint32_t maxWaitBetweenReconnectionRetries,
                                            uint32_t minWaitBetweenReconnectionRetries)
    {
        uint32_t waitTime = (retryCount < 26) ? (std::pow(2, retryCount) * 100) : 0;

        if (waitTime < minWaitBetweenReconnectionRetries)
        {
            waitTime = minWaitBetweenReconnectionRetries;
        }

        if (waitTime > maxWaitBetweenReconnectionRetries || waitTime == 0)
        {
            waitTime = maxWaitBetweenReconnectionRetries;
        }

        return waitTime;
    }
} // namespace ix
