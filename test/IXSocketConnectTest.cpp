/*
 *  IXSocketConnectTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include <catch_amalgamated.hpp>
#include <iostream>
#include <ixwebsocket/IXSocketConnect.h>

using namespace ix;

TEST_CASE("Write polling reports invalid socket errors", "[net][connect]")
{
    const auto fd = ::socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(fd != static_cast<socket_t>(-1));
    Socket::closeSocket(fd);
    Socket::setErrno(0);
    CHECK(Socket::poll(false, 0, fd, nullptr) == PollResultType::Error);
    CHECK(Socket::getErrno() != 0);
}

TEST_CASE("socket_connect", "[net]")
{
    SECTION("Test connecting to a known hostname")
    {
        int port = getFreePort();
        ix::WebSocketServer server(port);
        REQUIRE(startWebSocketEchoServer(server));

        std::string errMsg;
        int fd = SocketConnect::connect("127.0.0.1", port, errMsg, [] { return false; });
        std::cerr << "Error message: " << errMsg << std::endl;
        REQUIRE(fd != -1);
    }

    SECTION("Test connecting to a non-existing hostname")
    {
        std::string errMsg;
        // Use a reserved name and a trailing dot to avoid DNS search-domain expansion.
        std::string hostname("ixwebsocket-test.invalid.");
        int fd = SocketConnect::connect(hostname, 80, errMsg, [] { return false; });
        std::cerr << "Error message: " << errMsg << std::endl;
        REQUIRE(fd == -1);
    }

    SECTION("Test connecting to a good hostname, with cancellation")
    {
        int port = getFreePort();
        ix::WebSocketServer server(port);
        REQUIRE(startWebSocketEchoServer(server));

        std::string errMsg;
        // The callback returning true means we are requesting cancellation
        int fd = SocketConnect::connect("127.0.0.1", port, errMsg, [] { return true; });
        std::cerr << "Error message: " << errMsg << std::endl;
        REQUIRE(fd == -1);
    }
}
