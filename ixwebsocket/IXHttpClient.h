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

#include "IXSocket.h"
#include "IXWebSocketHttpHeaders.h"

namespace ix 
{
    using HttpResponse = std::tuple<int, WebSocketHttpHeaders, std::string>;

    class HttpClient {
    public:
        HttpClient();
        ~HttpClient();

        // Static methods ?
        HttpResponse get(const std::string& url);
        HttpResponse post(const std::string& url);

    private:
        Socket _socket;
    };
}
