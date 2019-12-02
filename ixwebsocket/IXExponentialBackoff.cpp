/*
 *  IXExponentialBackoff.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXExponentialBackoff.h"

#include <cmath>

namespace ix
{
    uint32_t calculateRetryWaitMilliseconds(uint32_t retry_count,
                                            uint32_t maxWaitBetweenReconnectionRetries)
    {
        uint32_t wait_time = (retry_count < 26) ? (std::pow(2, retry_count) * 100) : 0;

        if (wait_time > maxWaitBetweenReconnectionRetries || wait_time == 0)
        {
            wait_time = maxWaitBetweenReconnectionRetries;
        }

        return wait_time;
    }
} // namespace ix
