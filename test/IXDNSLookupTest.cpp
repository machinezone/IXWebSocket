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
    SECTION("Test resolving a known hostname")
    {
        auto dnsLookup = std::make_shared<DNSLookup>("www.google.com", 80);

        std::string errMsg;
        auto res = dnsLookup->resolve(errMsg, [] { return false; });
        std::cerr << "Error message: " << errMsg << std::endl;
        REQUIRE(res != nullptr);
    }

    SECTION("Test resolving a non-existing hostname")
    {
        // Use a reserved name and a trailing dot to avoid DNS search-domain expansion.
        auto dnsLookup = std::make_shared<DNSLookup>("ixwebsocket-test.invalid.", 80);

        std::string errMsg;
        auto res = dnsLookup->resolve(errMsg, [] { return false; });
        std::cerr << "Error message: " << errMsg << std::endl;
        REQUIRE(res == nullptr);
    }

    SECTION("Test resolving a good hostname, with cancellation")
    {
        auto dnsLookup = std::make_shared<DNSLookup>("www.google.com", 80, 1);

        std::string errMsg;
        // The callback returning true means we are requesting cancellation
        auto res = dnsLookup->resolve(errMsg, [] { return true; });
        std::cerr << "Error message: " << errMsg << std::endl;
        REQUIRE(res == nullptr);
    }
}
