/*
 *  IXHttpClient.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <algorithm>
#include <functional>
#include <mutex>
#include <atomic>
#include <tuple>
#include <memory>

#include "IXSocket.h"
#include "IXWebSocketHttpHeaders.h"

namespace ix 
{
    using HttpResponse = std::tuple<int, WebSocketHttpHeaders, std::string, std::string>;

    class HttpClient {
    public:
        HttpClient();
        ~HttpClient();

        // Static methods ?
        HttpResponse get(const std::string& url, bool verbose);

    private:
        std::shared_ptr<Socket> _socket;
    };
}
