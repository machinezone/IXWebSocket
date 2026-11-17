/*
 *  IXSocketConnectTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include <catch_amalgamated.hpp>
#include <iostream>
#include <ixwebsocket/IXSocketConnect.h>
#include <system_error>

using namespace ix;

TEST_CASE("Refused connections retain the socket error", "[net][connect]")
{
    const auto fd = ::socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(fd != static_cast<socket_t>(-1));
    struct CloseSocket
    {
        socket_t fd;
        ~CloseSocket()
        {
            if (fd != static_cast<socket_t>(-1)) Socket::closeSocket(fd);
        }
    } close {fd};
    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    REQUIRE(::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    socklen_t size = sizeof(address);
    REQUIRE(getsockname(fd, reinterpret_cast<sockaddr*>(&address), &size) == 0);
    Socket::closeSocket(fd);
    close.fd = static_cast<socket_t>(-1);
    std::string error;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    Socket::setErrno(0);
    const int connected =
        SocketConnect::connect("127.0.0.1",
                               ntohs(address.sin_port),
                               error,
                               [deadline] { return std::chrono::steady_clock::now() >= deadline; });
    REQUIRE(connected == -1);
    CAPTURE(error);
#ifdef _WIN32
    const int expected = WSAECONNREFUSED;
#else
    const int expected = ECONNREFUSED;
#endif
    CHECK(error.find(std::system_category().message(expected)) != std::string::npos);
}

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
