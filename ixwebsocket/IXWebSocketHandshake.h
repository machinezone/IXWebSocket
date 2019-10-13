/*
 *  IXWebSocketHandshake.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXCancellationRequest.h"
#include "IXSocket.h"
#include "IXWebSocketHttpHeaders.h"
#include "IXWebSocketInitResult.h"
#include "IXWebSocketPerMessageDeflate.h"
#include "IXWebSocketPerMessageDeflateOptions.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <string>

namespace ix
{
    class WebSocketHandshake
    {
    public:
        WebSocketHandshake(std::atomic<bool>& requestInitCancellation,
                           std::shared_ptr<Socket> _socket,
                           WebSocketPerMessageDeflate& perMessageDeflate,
                           WebSocketPerMessageDeflateOptions& perMessageDeflateOptions,
                           std::atomic<bool>& enablePerMessageDeflate);

        WebSocketInitResult clientHandshake(const std::string& url,
                                            const WebSocketHttpHeaders& extraHeaders,
                                            const std::string& host,
                                            const std::string& path,
                                            int port,
                                            int timeoutSecs);

        WebSocketInitResult serverHandshake(int timeoutSecs);

    private:
        std::string genRandomString(const int len);

        // Parse HTTP headers
        WebSocketInitResult sendErrorResponse(int code, const std::string& reason);

        bool insensitiveStringCompare(const std::string& a, const std::string& b);

        std::atomic<bool>& _requestInitCancellation;
        std::shared_ptr<Socket> _socket;
        WebSocketPerMessageDeflate& _perMessageDeflate;
        WebSocketPerMessageDeflateOptions& _perMessageDeflateOptions;
        std::atomic<bool>& _enablePerMessageDeflate;
    };
} // namespace ix
