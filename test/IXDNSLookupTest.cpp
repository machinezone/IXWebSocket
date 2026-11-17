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

    SECTION("Test resolving a non-existing hostname")
    {
        // Use a reserved name and a trailing dot to avoid DNS search-domain expansion.
        auto dnsLookup = std::make_shared<DNSLookup>("ixwebsocket-test.invalid.", 80);

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

TEST_CASE("Numeric addresses do not require a DNS worker", "[net][numeric_dns]")
{
    const std::string host = GENERATE(std::string("127.0.0.1"), std::string("::1"));
    DNSLookup lookup(host, 80);
    std::string error;
    auto result = lookup.resolve(error, CancellationRequest());
    REQUIRE(result != nullptr);
    CHECK(error.empty());
    CHECK(result->ai_family == (host == "::1" ? AF_INET6 : AF_INET));
}

TEST_CASE("Already cancelled DNS lookups do not start a worker", "[net][numeric_dns]")
{
    DNSLookup lookup("ixwebsocket-test.invalid.", 80);
    std::string error;
    CHECK(lookup.resolve(error, [] { return true; }) == nullptr);
    CHECK(error == "cancellation requested");
}

TEST_CASE("Numeric resolution preserves cancellation after parsing", "[net][numeric_dns]")
{
    DNSLookup lookup("127.0.0.1", 80);
    unsigned checks = 0;
    std::string error;
    CHECK(lookup.resolve(error, [&] { return ++checks > 1; }) == nullptr);
    CHECK(checks == 2);
    CHECK(error == "cancellation requested");
}

TEST_CASE("Synchronous hostname resolution accepts an empty cancellation callback",
          "[net][numeric_dns]")
{
    DNSLookup lookup("localhost", 80);
    std::string error;
    CHECK(lookup.resolve(error, CancellationRequest(), false) != nullptr);
}
