/*
 *  IXHttpClient.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXHttp.h"
#include "IXProxyOptions.h"
#include "IXSocket.h"
#include "IXSocketTLSOptions.h"
#include "IXWebSocketExport.h"
#include "IXWebSocketHttpHeaders.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

namespace ix
{
    class WebSocket;

    class HttpClient
    {
    public:
        HttpClient(bool async = false);
        ~HttpClient();

        HttpResponsePtr get(const std::string& url, HttpRequestArgsPtr args);
        HttpResponsePtr head(const std::string& url, HttpRequestArgsPtr args);
        HttpResponsePtr Delete(const std::string& url, HttpRequestArgsPtr args);

        HttpResponsePtr post(const std::string& url,
                             const HttpParameters& httpParameters,
                             const HttpFormDataParameters& httpFormDataParameters,
                             HttpRequestArgsPtr args);
        HttpResponsePtr post(const std::string& url,
                             const std::string& body,
                             HttpRequestArgsPtr args);

        HttpResponsePtr put(const std::string& url,
                            const HttpParameters& httpParameters,
                            const HttpFormDataParameters& httpFormDataParameters,
                            HttpRequestArgsPtr args);
        HttpResponsePtr put(const std::string& url,
                            const std::string& body,
                            HttpRequestArgsPtr args);

        HttpResponsePtr patch(const std::string& url,
                              const HttpParameters& httpParameters,
                              const HttpFormDataParameters& httpFormDataParameters,
                              HttpRequestArgsPtr args);
        HttpResponsePtr patch(const std::string& url,
                              const std::string& body,
                              HttpRequestArgsPtr args);

        HttpResponsePtr request(const std::string& url,
                                const std::string& verb,
                                const std::string& body,
                                HttpRequestArgsPtr args,
                                int redirects = 0);

        HttpResponsePtr request(const std::string& url,
                                const std::string& verb,
                                const HttpParameters& httpParameters,
                                const HttpFormDataParameters& httpFormDataParameters,
                                HttpRequestArgsPtr args);

        void setForceBody(bool value);

        // Async API
        HttpRequestArgsPtr createRequest(const std::string& url = std::string(),
                                         const std::string& verb = HttpClient::kGet);

        bool performRequest(HttpRequestArgsPtr request,
                            const OnResponseCallback& onResponseCallback);

        // TLS
        void setTLSOptions(const SocketTLSOptions& tlsOptions);

        // Configure the outbound SOCKS5 proxy before issuing requests.
        void setProxyOptions(const ProxyOptions& proxyOptions);

        std::string serializeHttpParameters(const HttpParameters& httpParameters);

        std::string serializeHttpFormDataParameters(
            const std::string& multipartBoundary,
            const HttpFormDataParameters& httpFormDataParameters,
            const HttpParameters& httpParameters = HttpParameters());

        std::string generateMultipartBoundary();

        std::string urlEncode(const std::string& value);

        IXWEBSOCKET_EXPORT const static std::string kPost;
        IXWEBSOCKET_EXPORT const static std::string kGet;
        IXWEBSOCKET_EXPORT const static std::string kHead;
        IXWEBSOCKET_EXPORT const static std::string kDelete;
        IXWEBSOCKET_EXPORT const static std::string kPut;
        IXWEBSOCKET_EXPORT const static std::string kPatch;

    private:
        friend struct HttpClientReuseTestAccess;
        friend class WebSocket;
        std::unique_ptr<Socket> takeConnection(const std::string& url,
                                               SocketTLSOptions& tlsOptions,
                                               ProxyOptions& proxyOptions,
                                               std::string& error);
        void log(const std::string& msg, HttpRequestArgsPtr args);

        HttpResponsePtr requestImpl(const std::string& url,
                                    const std::string& verb,
                                    const std::string& body,
                                    HttpRequestArgsPtr args,
                                    std::atomic<bool>& cancel,
                                    int redirects,
                                    bool allowRetry = true);


        // Async API background thread runner
        void run();
        // Async API
        bool _async;
        std::queue<std::function<void()>> _queue;
        mutable std::mutex _queueMutex;
        std::condition_variable _condition;
        std::atomic<bool> _stop;
        std::thread _thread;

        // One idle, fully consumed HTTP connection. Active requests own their socket locally.
        std::unique_ptr<Socket> _socket;
        std::string _connectionUrl;
        uint64_t _connectionConfigVersion = 0;
        std::recursive_mutex _mutex; // to protect accessing the _socket (only one socket per
                                     // client) the mutex needs to be recursive as this function
                                     // might be called recursively to follow HTTP redirections

        SocketTLSOptions _tlsOptions;
        ProxyOptions _proxyOptions;

        bool _forceBody;
    };
} // namespace ix
