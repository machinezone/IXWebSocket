#pragma once

#include <cstdint>
#include <ixwebsocket/IXProxyOptions.h>
#include <ixwebsocket/IXSocketTLSOptions.h>
#include <string>

namespace ix
{
    struct WebSocketBenchOptions
    {
        std::string url;
        uint64_t messages = 100000;
        unsigned runs = 3;
        unsigned warmup = 1;
        std::string callback = "both";
        int timeout = 30;
        bool json = false;
        bool quiet = false;
        bool compression = false;
        SocketTLSOptions tlsOptions;
        ProxyOptions proxyOptions;
    };

    int ws_bench(const WebSocketBenchOptions& options);
} // namespace ix
