/*
 *  ws_dns_lookup.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#include <atomic>
#include <ixwebsocket/IXDNSLookup.h>
#include <ixwebsocket/IXNetSystem.h>
#include <spdlog/spdlog.h>
#include <sstream>


namespace ix
{
    int ws_dns_lookup(const std::string& hostname)
    {
        auto dnsLookup = std::make_shared<DNSLookup>(hostname, 80);

        std::string errMsg;
        struct addrinfo* res;

        res = dnsLookup->resolve(errMsg, [] { return false; });

        auto addr = res->ai_addr;

        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr, str, INET_ADDRSTRLEN);

        spdlog::info("host: {} ip: {}", hostname, str);

        return 0;
    }
} // namespace ix
