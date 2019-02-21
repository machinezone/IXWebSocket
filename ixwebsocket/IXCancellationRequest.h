/*
 *  IXCancellationRequest.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <functional>
#include <atomic>

namespace ix
{
    using CancellationRequest = std::function<bool()>;

    CancellationRequest makeCancellationRequestWithTimeout(int seconds,
                                                           std::atomic<bool>& requestInitCancellation);
}

