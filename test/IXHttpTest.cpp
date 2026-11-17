/*
 *  IXHttpTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#include <algorithm>
#include <catch_amalgamated.hpp>
#include <clocale>
#include <iostream>
#include <ixwebsocket/IXHttp.h>
#include <ixwebsocket/IXHttpClient.h>
#include <string.h>
#include <thread>
#include <vector>

namespace ix
{
    TEST_CASE("URL encoding uses ASCII unreserved bytes in every locale", "[http][encode]")
    {
        HttpClient client;
        CHECK(client.urlEncode("AZaz09-._~ /+") == "AZaz09-._~%20%2F%2B");
        CHECK(client.urlEncode(std::string("\0\x7f\x80\xc4\xe4\xff", 6)) == "%00%7F%80%C4%E4%FF");
        struct RestoreLocale
        {
            std::string name = std::setlocale(LC_CTYPE, nullptr);
            ~RestoreLocale()
            {
                std::setlocale(LC_CTYPE, name.c_str());
            }
        } restore;
        if (!std::setlocale(LC_CTYPE, "en_US.ISO8859-1") &&
            !std::setlocale(LC_CTYPE, "tr_TR.ISO8859-9"))
            SKIP("No single-byte non-ASCII locale installed");
        CHECK(client.urlEncode("\xc4\xe4") == "%C4%E4");
        CHECK(client.urlEncode("\xc3\xa9") == "%C3%A9");
    }

    TEST_CASE("Multipart boundaries can be generated concurrently", "[http][multipart]")
    {
        std::vector<std::vector<std::string>> boundaries(4);
        std::vector<std::thread> workers;
        for (size_t i = 0; i < boundaries.size(); ++i)
            workers.emplace_back(
                [&, i]
                {
                    HttpClient client;
                    for (int j = 0; j < 32; ++j)
                        boundaries[i].push_back(client.generateMultipartBoundary());
                });
        for (auto& worker : workers)
            worker.join();
        const std::string alphabet =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        for (auto& group : boundaries)
            for (auto boundary : group)
            {
                std::sort(boundary.begin(), boundary.end());
                CHECK(boundary == alphabet);
            }
    }

    TEST_CASE("http", "[http]")
    {
        SECTION("Normal case")
        {
            std::string line = "HTTP/1.1 200";
            auto result = Http::parseStatusLine(line);

            REQUIRE(result.first == "HTTP/1.1");
            REQUIRE(result.second == 200);
        }

        SECTION("http/1.0 case")
        {
            std::string line = "HTTP/1.0 200";
            auto result = Http::parseStatusLine(line);

            REQUIRE(result.first == "HTTP/1.0");
            REQUIRE(result.second == 200);
        }

        SECTION("empty case")
        {
            std::string line = "";
            auto result = Http::parseStatusLine(line);

            REQUIRE(result.first == "");
            REQUIRE(result.second == -1);
        }

        SECTION("empty case")
        {
            std::string line = "HTTP/1.1";
            auto result = Http::parseStatusLine(line);

            REQUIRE(result.first == "HTTP/1.1");
            REQUIRE(result.second == -1);
        }
    }

} // namespace ix
