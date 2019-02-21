/*
 *  IXWebSocketHandshake.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXCancellationRequest.h"
#include "IXWebSocketHttpHeaders.h"
#include "IXWebSocketPerMessageDeflate.h"
#include "IXWebSocketPerMessageDeflateOptions.h"
#include "IXSocket.h"

#include <string>
#include <atomic>
#include <chrono>
#include <memory>
#include <tuple>

namespace ix
{
    struct WebSocketInitResult
    {
        bool success;
        int http_status;
        std::string errorStr;
        WebSocketHttpHeaders headers;
        std::string uri;

        WebSocketInitResult(bool s = false,
                            int status = 0,
                            const std::string& e = std::string(),
                            WebSocketHttpHeaders h = WebSocketHttpHeaders(),
                            const std::string& u = std::string())
        {
            success = s;
            http_status = status;
            errorStr = e;
            headers = h;
            uri = u;
        }
    };

    class WebSocketHandshake {
    public:
        WebSocketHandshake(std::atomic<bool>& requestInitCancellation,
                           std::shared_ptr<Socket> _socket,
                           WebSocketPerMessageDeflate& perMessageDeflate,
                           WebSocketPerMessageDeflateOptions& perMessageDeflateOptions,
                           std::atomic<bool>& enablePerMessageDeflate);

        WebSocketInitResult clientHandshake(const std::string& url,
                                            const std::string& host,
                                            const std::string& path,
                                            int port,
                                            int timeoutSecs);

        WebSocketInitResult serverHandshake(int fd,
                                            int timeoutSecs);

    private:
        std::string genRandomString(const int len);

        // Parse HTTP headers
        WebSocketInitResult sendErrorResponse(int code, const std::string& reason);

        std::tuple<std::string, std::string, std::string> parseRequestLine(const std::string& line);
        std::string trim(const std::string& str);
        bool insensitiveStringCompare(const std::string& a, const std::string& b);

        std::atomic<bool>& _requestInitCancellation;
        std::shared_ptr<Socket> _socket;
        WebSocketPerMessageDeflate& _perMessageDeflate;
        WebSocketPerMessageDeflateOptions& _perMessageDeflateOptions;
        std::atomic<bool>& _enablePerMessageDeflate;
    };
}
