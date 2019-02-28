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

    struct HttpRequestArgs
    {
        std::string url;
        WebSocketHttpHeaders extraHeaders;
        std::string body;
        int timeoutSecs;
        bool followRedirects;
        bool verbose;
        bool compress;
    };

    class HttpClient {
    public:
        HttpClient();
        ~HttpClient();

        HttpResponse get(HttpRequestArgs args);
        HttpResponse head(HttpRequestArgs args);

        HttpResponse post(const HttpParameters& httpParameters,
                          HttpRequestArgs args);
        HttpResponse post(const std::string& body,
                          HttpRequestArgs args);

    private:
        HttpResponse request(const std::string& verb,
                             const std::string& body,
                             HttpRequestArgs args);

        std::string serializeHttpParameters(const HttpParameters& httpParameters);

        std::string urlEncode(const std::string& value);

        bool gzipInflate(
            const std::string& in,
            std::string& out);

        std::shared_ptr<Socket> _socket;
    };
}
