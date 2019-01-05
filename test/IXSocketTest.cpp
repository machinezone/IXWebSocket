/*
 *  IXSocketTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#include <iostream>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXCancellationRequest.h>

#if defined(__APPLE__) or defined(__linux__)
# ifdef __APPLE__
#  include <ixwebsocket/IXSocketAppleSSL.h>
# else
#  include <ixwebsocket/IXSocketOpenSSL.h>
# endif
#endif

#include "IXTest.h" 
#include "catch.hpp"

using namespace ix;

namespace ix
{
    void testSocket(const std::string& host,
                    int port,
                    const std::string& request,
                    std::shared_ptr<Socket> socket,
                    int expectedStatus,
                    int timeoutSecs)
    {
        std::string errMsg;
        static std::atomic<bool> requestInitCancellation(false);
        auto isCancellationRequested =
            makeCancellationRequestWithTimeout(timeoutSecs, requestInitCancellation);

        bool success = socket->connect(host, port, errMsg, isCancellationRequested);
        REQUIRE(success);

        std::cout << "Sending request: " << request
                  << "to " << host << ":" << port
                  << std::endl;
        socket->writeBytes(request, isCancellationRequested);

        auto lineResult = socket->readLine(isCancellationRequested);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        REQUIRE(lineValid);

        int status = -1;
        REQUIRE(sscanf(line.c_str(), "HTTP/1.1 %d", &status) == 1);
        REQUIRE(status == expectedStatus);
    }
}

TEST_CASE("socket", "[socket]")
{
    SECTION("Connect to google HTTP server. Send GET request without header. Should return 200")
    {
        std::shared_ptr<Socket> socket(new Socket);
        std::string host("www.google.com");
        int port = 80;
        std::string request("GET / HTTP/1.1\r\n\r\n");
        int expectedStatus = 200;
        int timeoutSecs = 1;

        testSocket(host, port, request, socket, expectedStatus, timeoutSecs);
    }

#if defined(__APPLE__) or defined(__linux__)
    SECTION("Connect to google HTTPS server. Send GET request without header. Should return 200")
    {
# ifdef __APPLE__
        std::shared_ptr<Socket> socket = std::make_shared<SocketAppleSSL>();
# else
        std::shared_ptr<Socket> socket = std::make_shared<SocketOpenSSL>();
# endif
        std::string host("www.google.com");
        int port = 443;
        std::string request("GET / HTTP/1.1\r\n\r\n");
        int expectedStatus = 200;
        int timeoutSecs = 1;

        testSocket(host, port, request, socket, expectedStatus, timeoutSecs);
    }
#endif
}
