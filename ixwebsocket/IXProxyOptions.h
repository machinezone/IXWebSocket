/*
 *  IXProxyOptions.h
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#pragma once

#include <string>

namespace ix
{
    struct ProxyOptions
    {
        enum class DnsMode
        {
            Local,
            Remote
        };

        // An empty host disables the SOCKS5 proxy. Configure before connecting.
        std::string host;
        int port = 1080;
        DnsMode dnsMode = DnsMode::Remote;

        // Optional username/password authentication; supply both fields.
        std::string username;
        std::string password;
    };
} // namespace ix
