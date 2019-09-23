/*
 *  IXExponentialBackoff.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <cstdint>

namespace ix
{
    uint32_t calculateRetryWaitMilliseconds(uint32_t retry_count,
                                            uint32_t maxWaitBetweenReconnectionRetries);
} // namespace ix
