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
#include <map>

#include "IXSocket.h"
#include "IXWebSocketHttpHeaders.h"

namespace ix
{
    using HttpResponse = std::tuple<int, WebSocketHttpHeaders, std::string, std::string>;
    using HttpParameters = std::map<std::string, std::string>;

    class HttpClient {
    public:
        HttpClient();
        ~HttpClient();

        // Static methods ?
        HttpResponse get(const std::string& url,
                         const WebSocketHttpHeaders& extraHeaders,
                         bool verbose);
        HttpResponse post(const std::string& url,
                          const WebSocketHttpHeaders& extraHeaders,
                          const HttpParameters& httpParameters,
                          bool verbose);

    private:
        HttpResponse request(const std::string& url,
                             const std::string& verb,
                             const WebSocketHttpHeaders& extraHeaders,
                             const HttpParameters& httpParameters,
                             bool verbose);

        std::string urlEncode(const std::string& value);

        std::shared_ptr<Socket> _socket;
    };
}
