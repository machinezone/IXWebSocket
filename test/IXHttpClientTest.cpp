/*
 *  IXSocketTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#include <iostream>
#include <ixwebsocket/IXHttpClient.h>

#include "catch.hpp"

using namespace ix;

TEST_CASE("http client", "[http]")
{
    SECTION("Connect to a remote HTTP server")
    {
        std::string url("http://httpbin.org/");

        WebSocketHttpHeaders headers;
        headers["User-Agent"] = "ixwebsocket";

        HttpRequestArgs args;
        args.extraHeaders = headers;
        args.connectTimeout = 60;
        args.transferTimeout = 60;
        args.followRedirects = true;
        args.maxRedirects = 10;
        args.verbose = true;
        args.compress = true;
        args.logger = [](const std::string& msg)
        {
            std::cout << msg;
        };
        args.onProgressCallback = [](int current, int total) -> bool
        {
            std::cerr << "\r" << "Downloaded "
                      << current << " bytes out of " << total;
            return true;
        };

        HttpClient httpClient;
        HttpResponse out;
        out = httpClient.get(url, args);

        auto statusCode = std::get<0>(out);
        auto errorCode = std::get<1>(out);
        auto responseHeaders = std::get<2>(out);
        auto payload = std::get<3>(out);
        auto errorMsg = std::get<4>(out);
        auto uploadSize = std::get<5>(out);
        auto downloadSize = std::get<6>(out);

        for (auto it : responseHeaders)
        {
            std::cerr << it.first << ": " << it.second << std::endl;
        }

        std::cerr << "Upload size: " << uploadSize << std::endl;
        std::cerr << "Download size: " << downloadSize << std::endl;
        std::cerr << "Status: " << statusCode << std::endl;

        REQUIRE(errorCode == HttpErrorCode::Ok);
    }

#if defined(IXWEBSOCKET_USE_TLS)
    SECTION("Connect to a remote HTTPS server")
    {
        std::string url("https://httpbin.org/");

        WebSocketHttpHeaders headers;
        headers["User-Agent"] = "ixwebsocket";

        HttpRequestArgs args;
        args.extraHeaders = headers;
        args.connectTimeout = 60;
        args.transferTimeout = 60;
        args.followRedirects = true;
        args.maxRedirects = 10;
        args.verbose = true;
        args.compress = true;
        args.logger = [](const std::string& msg)
        {
            std::cout << msg;
        };
        args.onProgressCallback = [](int current, int total) -> bool
        {
            std::cerr << "\r" << "Downloaded "
                      << current << " bytes out of " << total;
            return true;
        };

        HttpClient httpClient;
        HttpResponse out;
        out = httpClient.get(url, args);

        auto statusCode = std::get<0>(out);
        auto errorCode = std::get<1>(out);
        auto responseHeaders = std::get<2>(out);
        auto payload = std::get<3>(out);
        auto errorMsg = std::get<4>(out);
        auto uploadSize = std::get<5>(out);
        auto downloadSize = std::get<6>(out);

        for (auto it : responseHeaders)
        {
            std::cerr << it.first << ": " << it.second << std::endl;
        }

        std::cerr << "Upload size: " << uploadSize << std::endl;
        std::cerr << "Download size: " << downloadSize << std::endl;
        std::cerr << "Status: " << statusCode << std::endl;

        REQUIRE(errorCode == HttpErrorCode::Ok);
    }
#endif
}
