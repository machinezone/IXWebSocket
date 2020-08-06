/*
 *  IXHttpClient.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXHttp.h"
#include "IXSocket.h"
#include "IXSocketTLSOptions.h"
#include "IXWebSocketHttpHeaders.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>

namespace ix
{
    class HttpClient
    {
    public:
        HttpClient(bool async = false);
        ~HttpClient();

        HttpResponsePtr get(const std::string& url, HttpRequestArgsPtr args);
        HttpResponsePtr head(const std::string& url, HttpRequestArgsPtr args);
        HttpResponsePtr del(const std::string& url, HttpRequestArgsPtr args);

        HttpResponsePtr post(const std::string& url,
                             const HttpParameters& httpParameters,
                             HttpRequestArgsPtr args);
        HttpResponsePtr post(const std::string& url,
                             const std::string& body,
                             HttpRequestArgsPtr args);

        HttpResponsePtr put(const std::string& url,
                            const HttpParameters& httpParameters,
                            HttpRequestArgsPtr args);
        HttpResponsePtr put(const std::string& url,
                            const std::string& body,
                            HttpRequestArgsPtr args);

        HttpResponsePtr request(const std::string& url,
                                const std::string& verb,
                                const std::string& body,
                                HttpRequestArgsPtr args,
                                int redirects = 0);

		HttpResponsePtr request(const std::string& url,
								const std::string& verb,
								std::istream* body,
								HttpRequestArgsPtr args,
								size_t bufferSize,
								int redirects = 0);

        // Async API
        HttpRequestArgsPtr createRequest(const std::string& url = std::string(),
                                         const std::string& verb = HttpClient::kGet);
        HttpRequestArgsPtr createStreamRequest(std::istream* stream, size_t bufferSize = 4096, const std::string& url = std::string(),
											   const std::string& verb = HttpClient::kGet);

        bool performRequest(HttpRequestArgsPtr request,
                            const OnResponseCallback& onResponseCallback);

        // TLS
        void setTLSOptions(const SocketTLSOptions& tlsOptions);

        std::string serializeHttpParameters(const HttpParameters& httpParameters);

        std::string serializeHttpFormDataParameters(
            const std::string& multipartBoundary,
            const HttpFormDataParameters& httpFormDataParameters,
            const HttpParameters& httpParameters = HttpParameters());

        std::string generateMultipartBoundary();

        std::string urlEncode(const std::string& value);

        const static std::string kPost;
        const static std::string kGet;
        const static std::string kHead;
        const static std::string kDel;
        const static std::string kPut;

    private:
        void log(const std::string& msg, HttpRequestArgsPtr args);

        bool gzipInflate(const std::string& in, std::string& out);

		struct RequestData
        {
            std::string protocol, host, path, query;
            int port;

            uint64_t uploadSize = 0;
            uint64_t downloadSize = 0;
            int code = 0;
            WebSocketHttpHeaders headers;
            std::string payload;
            std::string description;
            std::string req;
            CancellationRequest isCancellationRequested;
            int redirects = 0;
            std::string errorMsg;
            std::stringstream ss;

			std::function<HttpResponsePtr()> redirect;
        };
        HttpResponsePtr pre_request(RequestData& data,
                         const std::string& url,
                         const std::string& verb,
                         HttpRequestArgsPtr args,
                         int redirects = 0);
        HttpResponsePtr post_request(RequestData& data,
									 const std::string& url,
                                     const std::string& verb,
                                     HttpRequestArgsPtr args,
                                     int redirects = 0);

        // Async API background thread runner
        void run();

        // Async API
        bool _async;
        std::queue<std::pair<HttpRequestArgsPtr, OnResponseCallback>> _queue;
        mutable std::mutex _queueMutex;
        std::condition_variable _condition;
        std::atomic<bool> _stop;
        std::thread _thread;

        std::unique_ptr<Socket> _socket;
        std::mutex _mutex; // to protect accessing the _socket (only one socket per client)

        SocketTLSOptions _tlsOptions;
    };
} // namespace ix
