/*
 *  IXSocketTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include <algorithm>
#include <catch_amalgamated.hpp>
#include <cstring>
#include <iostream>
#include <ixwebsocket/IXCancellationRequest.h>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXSocketFactory.h>
#include <string.h>
#include <thread>

using namespace ix;

namespace
{
    // No native descriptor: completed reads and EOF must not poll for more data.
    class MemorySocket : public Socket
    {
    public:
        explicit MemorySocket(const std::string& data = std::string(), int error = 0)
            : _data(data)
            , _error(error)
        {
        }

        std::ptrdiff_t recv(void* buffer, size_t length) override
        {
            ++reads;
            if (_data.empty())
            {
                Socket::setErrno(_error);
                return 0;
            }
            const size_t count = (std::min) (length, _data.size());
            std::memcpy(buffer, _data.data(), count);
            _data.erase(0, count);
            return static_cast<std::ptrdiff_t>(count);
        }

        unsigned reads = 0;

    private:
        std::string _data;
        int _error;
    };
} // namespace

TEST_CASE("Completed socket reads do not wait for more data", "[socket][read]")
{
    const bool streaming = GENERATE(false, true);
    MemorySocket socket("abc");
    std::string streamed;
    OnChunkCallback onChunk;
    if (streaming) onChunk = [&](const std::string& bytes) { streamed += bytes; };
    int progress = 0;
    auto result = socket.readBytes(
        3,
        [&](int current, int total)
        {
            CHECK(current == 3);
            CHECK(total == 3);
            ++progress;
            return true;
        },
        onChunk,
        nullptr);
    REQUIRE(result.first);
    CHECK(result.second == (streaming ? "" : "abc"));
    CHECK(streamed == (streaming ? "abc" : ""));
    CHECK(progress == 1);
    CHECK(socket.reads == 1);
}

TEST_CASE("Socket EOF does not depend on the previous socket error", "[socket][read]")
{
    const int error = GENERATE(0, EWOULDBLOCK);
    MemorySocket socket("", error);
    unsigned progress = 0;
    auto result = socket.readBytes(
        1,
        [&](int, int)
        {
            ++progress;
            return true;
        },
        nullptr,
        nullptr);
    CHECK_FALSE(result.first);
    CHECK(progress == 0);
    CHECK(socket.reads == 1);
}

TEST_CASE("Empty and cancelled socket reads do not receive data", "[socket][read]")
{
    MemorySocket socket("abc");
    auto empty = socket.readBytes(0, nullptr, nullptr, nullptr);
    CHECK(empty.first);
    CHECK(empty.second.empty());
    CHECK(socket.reads == 0);
    auto cancelled = socket.readBytes(3, nullptr, nullptr, [] { return true; });
    CHECK_FALSE(cancelled.first);
    CHECK(socket.reads == 0);
}

TEST_CASE("Socket error state remains local when threads are reused", "[socket][errno]")
{
    std::atomic<bool> isolated {true};
    auto churn = [&](int error)
    {
        for (int i = 0; i < 16; ++i)
        {
            std::thread worker(
                [&, error]
                {
                    Socket::setErrno(error);
                    if (Socket::getErrno() != error) isolated = false;
                });
            worker.join();
        }
    };
    std::thread a(churn, EWOULDBLOCK);
    std::thread b(churn, EINVAL);
    a.join();
    b.join();
    CHECK(isolated.load());
}

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
        TLogger() << "errMsg: " << errMsg;
        REQUIRE(success);

        TLogger() << "Sending request: " << request << "to " << host << ":" << port;
        REQUIRE(socket->writeBytes(request, isCancellationRequested));

        auto lineResult = socket->readLine(isCancellationRequested);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        TLogger() << "read error: " << strerror(Socket::getErrno());

        REQUIRE(lineValid);

        int status = -1;
        REQUIRE(sscanf(line.c_str(), "HTTP/1.1 %d", &status) == 1);
        REQUIRE(status == expectedStatus);
    }
} // namespace ix

TEST_CASE("socket", "[socket]")
{
    SECTION("Connect to a local websocket server over a free port. Send GET request without "
            "header. Should return 400")
    {
        // Start a server first which we'll hit with our socket code
        int port = getFreePort();
        ix::WebSocketServer server(port);
        REQUIRE(startWebSocketEchoServer(server));

        std::string errMsg;
        bool tls = false;
        SocketTLSOptions tlsOptions;
        std::shared_ptr<Socket> socket = createSocket(tls, -1, errMsg, tlsOptions);
        std::string host("127.0.0.1");

        std::stringstream ss;
        ss << "GET / HTTP/1.1\r\n";
        ss << "Host: " << host << "\r\n";
        ss << "\r\n";
        std::string request(ss.str());

        int expectedStatus = 400;
        int timeoutSecs = 3;

        testSocket(host, port, request, socket, expectedStatus, timeoutSecs);
    }
}
