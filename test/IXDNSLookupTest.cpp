/*
 *  IXDNSLookupTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include <catch_amalgamated.hpp>
#include <iostream>
#include <ixwebsocket/IXDNSLookup.h>

using namespace ix;


TEST_CASE("dns", "[net]")
{
    SECTION("Test resolving a loopback address")
    {
        auto dnsLookup = std::make_shared<DNSLookup>("127.0.0.1", 80);

        std::string errMsg;
        auto res = dnsLookup->resolve(errMsg, [] { return false; });
        std::cerr << "Error message: " << errMsg << std::endl;
        REQUIRE(res != nullptr);
    }

    SECTION("Test resolving an invalid IPv6 literal")
    {
        auto dnsLookup = std::make_shared<DNSLookup>("::invalid", 80);

        std::string errMsg;
        auto res = dnsLookup->resolve(errMsg, [] { return false; });
        std::cerr << "Error message: " << errMsg << std::endl;
        REQUIRE(res == nullptr);
    }

    SECTION("Test resolving a loopback address, with cancellation")
    {
        auto dnsLookup = std::make_shared<DNSLookup>("127.0.0.1", 80, 1);

        std::string errMsg;
        // The callback returning true means we are requesting cancellation
        auto res = dnsLookup->resolve(errMsg, [] { return true; });
        std::cerr << "Error message: " << errMsg << std::endl;
        REQUIRE(res == nullptr);
    }
}
