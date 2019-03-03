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
    enum HttpErrorCode
    {
        HttpErrorCode_Ok = 0,
        HttpErrorCode_CannotConnect = 1,
        HttpErrorCode_Timeout = 2,
        HttpErrorCode_Gzip = 3,
        HttpErrorCode_UrlMalformed = 4,
        HttpErrorCode_CannotCreateSocket = 5,
        HttpErrorCode_SendError = 6,
        HttpErrorCode_ReadError = 7,
        HttpErrorCode_CannotReadStatusLine = 8,
        HttpErrorCode_MissingStatus = 9,
        HttpErrorCode_HeaderParsingError = 10,
        HttpErrorCode_MissingLocation = 11,
        HttpErrorCode_TooManyRedirects = 12,
        HttpErrorCode_ChunkReadError = 13,
        HttpErrorCode_CannotReadBody = 14
    };

    using HttpResponse = std::tuple<int, // status
                                    HttpErrorCode, // error code
                                    WebSocketHttpHeaders,
                                    std::string, // payload
                                    std::string, // error msg
                                    uint64_t,    // upload size
                                    uint64_t>;   // download size

    using HttpParameters = std::map<std::string, std::string>;
    using Logger = std::function<void(const std::string&)>;

    struct HttpRequestArgs
    {
        std::string url;
        WebSocketHttpHeaders extraHeaders;
        std::string body;
        int connectTimeout;
        int transferTimeout;
        bool followRedirects;
        int maxRedirects;
        bool verbose;
        bool compress;
        Logger logger;
        OnProgressCallback onProgressCallback;
    };

    class HttpClient {
    public:
        HttpClient();
        ~HttpClient();

        HttpResponse get(const std::string& url,
                         const HttpRequestArgs& args);
        HttpResponse head(const std::string& url,
                          const HttpRequestArgs& args);

        HttpResponse post(const std::string& url,
                          const HttpParameters& httpParameters,
                          const HttpRequestArgs& args);
        HttpResponse post(const std::string& url,
                          const std::string& body,
                          const HttpRequestArgs& args);

    private:
        HttpResponse request(const std::string& url,
                             const std::string& verb,
                             const std::string& body,
                             const HttpRequestArgs& args,
                             int redirects = 0);

        std::string serializeHttpParameters(const HttpParameters& httpParameters);

        std::string urlEncode(const std::string& value);

        void log(const std::string& msg, const HttpRequestArgs& args);

        bool gzipInflate(
            const std::string& in,
            std::string& out);

        std::shared_ptr<Socket> _socket;

        const static std::string kPost;
        const static std::string kGet;
        const static std::string kHead;
    };
}
