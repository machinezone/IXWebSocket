/*
 *  IXHttpClient.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXSocket.h"
#include "IXWebSocketHttpHeaders.h"
#include <algorithm>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>

namespace ix
{
    enum class HttpErrorCode : int
    {
        Ok = 0,
        CannotConnect = 1,
        Timeout = 2,
        Gzip = 3,
        UrlMalformed = 4,
        CannotCreateSocket = 5,
        SendError = 6,
        ReadError = 7,
        CannotReadStatusLine = 8,
        MissingStatus = 9,
        HeaderParsingError = 10,
        MissingLocation = 11,
        TooManyRedirects = 12,
        ChunkReadError = 13,
        CannotReadBody = 14
    };

    struct HttpResponse
    {
        int statusCode;
        HttpErrorCode errorCode;
        WebSocketHttpHeaders headers;
        std::string payload;
        std::string errorMsg;
        uint64_t uploadSize;
        uint64_t downloadSize;

        HttpResponse(int s = 0,
                     const HttpErrorCode& c = HttpErrorCode::Ok,
                     const WebSocketHttpHeaders& h = WebSocketHttpHeaders(),
                     const std::string& p = std::string(),
                     const std::string& e = std::string(),
                     uint64_t u = 0,
                     uint64_t d = 0)
            : statusCode(s)
            , errorCode(c)
            , headers(h)
            , payload(p)
            , errorMsg(e)
            , uploadSize(u)
            , downloadSize(d)
        {
            ;
        }
    };

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

    class HttpClient
    {
    public:
        HttpClient();
        ~HttpClient();

        HttpResponse get(const std::string& url, const HttpRequestArgs& args);
        HttpResponse head(const std::string& url, const HttpRequestArgs& args);
        HttpResponse del(const std::string& url, const HttpRequestArgs& args);

        HttpResponse post(const std::string& url,
                          const HttpParameters& httpParameters,
                          const HttpRequestArgs& args);
        HttpResponse post(const std::string& url,
                          const std::string& body,
                          const HttpRequestArgs& args);

        HttpResponse put(const std::string& url,
                         const HttpParameters& httpParameters,
                         const HttpRequestArgs& args);
        HttpResponse put(const std::string& url,
                         const std::string& body,
                         const HttpRequestArgs& args);

        HttpResponse request(const std::string& url,
                             const std::string& verb,
                             const std::string& body,
                             const HttpRequestArgs& args,
                             int redirects = 0);

        std::string serializeHttpParameters(const HttpParameters& httpParameters);

        std::string urlEncode(const std::string& value);

    private:
        void log(const std::string& msg, const HttpRequestArgs& args);

        bool gzipInflate(const std::string& in, std::string& out);

        std::shared_ptr<Socket> _socket;

        const static std::string kPost;
        const static std::string kGet;
        const static std::string kHead;
        const static std::string kDel;
        const static std::string kPut;
    };
} // namespace ix
