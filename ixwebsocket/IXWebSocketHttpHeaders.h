/*
 *  IXWebSocketHttpHeaders.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXCancellationRequest.h"
#include "IXStrCaseCompare.h"
#include <map>
#include <memory>
#include <string>

namespace ix
{
    class Socket;

    using WebSocketHttpHeaders = std::map<std::string, std::string, CaseInsensitiveLess>;

    // Match a comma-separated header token, ignoring case and surrounding whitespace.
    bool hasHttpHeaderToken(const WebSocketHttpHeaders& headers,
                            const std::string& name,
                            const std::string& token);

    std::pair<bool, WebSocketHttpHeaders> parseHttpHeaders(
        std::unique_ptr<Socket>& socket, const CancellationRequest& isCancellationRequested);
} // namespace ix
