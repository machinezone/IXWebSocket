/*
 *  IXWebSocketHttpHeaders.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXCancellationRequest.h"

#include <string>
#include <unordered_map>
#include <memory>
#include <algorithm>

namespace ix
{
    class Socket;

    using WebSocketHttpHeaders = std::unordered_map<std::string, std::string>;

    std::pair<bool, WebSocketHttpHeaders> parseHttpHeaders(
        std::shared_ptr<Socket> socket,
        const CancellationRequest& isCancellationRequested);
}
