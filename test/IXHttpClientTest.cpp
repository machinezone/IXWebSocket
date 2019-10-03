/*
 *  IXSocketTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#include "catch.hpp"
#include <iostream>
#include <ixwebsocket/IXHttpClient.h>

using namespace ix;

TEST_CASE("http_client", "[http]")
{
    SECTION("Connect to a remote HTTP server")
    {
        HttpClient httpClient;
        WebSocketHttpHeaders headers;

        std::string url("http://httpbin.org/");
        auto args = httpClient.createRequest(url);

        args->extraHeaders = headers;
        args->connectTimeout = 60;
        args->transferTimeout = 60;
        args->followRedirects = true;
        args->maxRedirects = 10;
        args->verbose = true;
        args->compress = true;
        args->logger = [](const std::string& msg) { std::cout << msg; };
        args->onProgressCallback = [](int current, int total) -> bool {
            std::cerr << "\r"
                      << "Downloaded " << current << " bytes out of " << total;
            return true;
        };

        auto response = httpClient.get(url, args);

        for (auto it : response->headers)
        {
            std::cerr << it.first << ": " << it.second << std::endl;
        }

        std::cerr << "Upload size: " << response->uploadSize << std::endl;
        std::cerr << "Download size: " << response->downloadSize << std::endl;
        std::cerr << "Status: " << response->statusCode << std::endl;
        std::cerr << "Error message: " << response->errorMsg << std::endl;

        REQUIRE(response->errorCode == HttpErrorCode::Ok);
        REQUIRE(response->statusCode == 200);
    }

#ifdef IXWEBSOCKET_USE_TLS
    SECTION("Connect to a remote HTTPS server")
    {
        HttpClient httpClient;
        WebSocketHttpHeaders headers;

        SocketTLSOptions tlsOptions;
        tlsOptions.caFile = "cacert.pem";
        httpClient.setTLSOptions(tlsOptions);

        std::string url("https://httpbin.org/");
        auto args = httpClient.createRequest(url);

        args->extraHeaders = headers;
        args->connectTimeout = 60;
        args->transferTimeout = 60;
        args->followRedirects = true;
        args->maxRedirects = 10;
        args->verbose = true;
        args->compress = true;
        args->logger = [](const std::string& msg) { std::cout << msg; };
        args->onProgressCallback = [](int current, int total) -> bool {
            std::cerr << "\r"
                      << "Downloaded " << current << " bytes out of " << total;
            return true;
        };

        auto response = httpClient.get(url, args);

        for (auto it : response->headers)
        {
            std::cerr << it.first << ": " << it.second << std::endl;
        }

        std::cerr << "Upload size: " << response->uploadSize << std::endl;
        std::cerr << "Download size: " << response->downloadSize << std::endl;
        std::cerr << "Status: " << response->statusCode << std::endl;
        std::cerr << "Error message: " << response->errorMsg << std::endl;

        REQUIRE(response->errorCode == HttpErrorCode::Ok);
        REQUIRE(response->statusCode == 200);
    }
#endif

    SECTION("Async API, one call")
    {
        bool async = true;
        HttpClient httpClient(async);
        WebSocketHttpHeaders headers;

        SocketTLSOptions tlsOptions;
        tlsOptions.caFile = "cacert.pem";
        httpClient.setTLSOptions(tlsOptions);

        std::string url("http://httpbin.org/");
        auto args = httpClient.createRequest(url);

        args->extraHeaders = headers;
        args->connectTimeout = 60;
        args->transferTimeout = 60;
        args->followRedirects = true;
        args->maxRedirects = 10;
        args->verbose = true;
        args->compress = true;
        args->logger = [](const std::string& msg) { std::cout << msg; };
        args->onProgressCallback = [](int current, int total) -> bool {
            std::cerr << "\r"
                      << "Downloaded " << current << " bytes out of " << total;
            return true;
        };

        std::atomic<bool> requestCompleted(false);
        std::atomic<int> statusCode(0);

        httpClient.performRequest(
            args, [&requestCompleted, &statusCode](const HttpResponsePtr& response) {
                std::cerr << "Upload size: " << response->uploadSize << std::endl;
                std::cerr << "Download size: " << response->downloadSize << std::endl;
                std::cerr << "Status: " << response->statusCode << std::endl;
                std::cerr << "Error message: " << response->errorMsg << std::endl;

                // In case of failure, print response->errorMsg
                statusCode = response->statusCode;
                requestCompleted = true;
            });

        int wait = 0;
        while (wait < 5000)
        {
            if (requestCompleted) break;

            std::chrono::duration<double, std::milli> duration(10);
            std::this_thread::sleep_for(duration);
            wait += 10;
        }

        std::cerr << "Done" << std::endl;
        REQUIRE(statusCode == 200);
    }

    SECTION("Async API, multiple calls")
    {
        bool async = true;
        HttpClient httpClient(async);
        WebSocketHttpHeaders headers;

        std::string url("http://httpbin.org/");
        auto args = httpClient.createRequest(url);

        args->extraHeaders = headers;
        args->connectTimeout = 60;
        args->transferTimeout = 60;
        args->followRedirects = true;
        args->maxRedirects = 10;
        args->verbose = true;
        args->compress = true;
        args->logger = [](const std::string& msg) { std::cout << msg; };
        args->onProgressCallback = [](int current, int total) -> bool {
            std::cerr << "\r"
                      << "Downloaded " << current << " bytes out of " << total;
            return true;
        };

        std::atomic<bool> requestCompleted(false);
        std::atomic<int> statusCode0(0);
        std::atomic<int> statusCode1(0);
        std::atomic<int> statusCode2(0);

        for (int i = 0; i < 3; ++i)
        {
            httpClient.performRequest(
                args,
                [i, &requestCompleted, &statusCode0, &statusCode1, &statusCode2](
                    const HttpResponsePtr& response) {
                    std::cerr << "Upload size: " << response->uploadSize << std::endl;
                    std::cerr << "Download size: " << response->downloadSize << std::endl;
                    std::cerr << "Status: " << response->statusCode << std::endl;
                    std::cerr << "Error message: " << response->errorMsg << std::endl;

                    // In case of failure, print response->errorMsg
                    if (i == 0)
                    {
                        statusCode0 = response->statusCode;
                    }
                    else if (i == 1)
                    {
                        statusCode1 = response->statusCode;
                    }
                    else if (i == 2)
                    {
                        statusCode2 = response->statusCode;
                        requestCompleted = true;
                    }
                });
        }

        int wait = 0;
        while (wait < 10000)
        {
            if (requestCompleted) break;

            std::chrono::duration<double, std::milli> duration(10);
            std::this_thread::sleep_for(duration);
            wait += 10;
        }

        std::cerr << "Done" << std::endl;
        REQUIRE(statusCode0 == 200);
        REQUIRE(statusCode1 == 200);
        REQUIRE(statusCode2 == 200);
    }
}
