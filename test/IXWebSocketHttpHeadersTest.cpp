/*
 *  IXWebSocketHttpHeadersTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */

#include <algorithm>
#include <catch_amalgamated.hpp>
#include <chrono>
#include <cstring>
#include <future>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXHttp.h>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketHttpHeaders.h>
#include <ixwebsocket/IXWebSocketServer.h>

namespace
{
    class HttpHeadersTestSocket final : public ix::Socket
    {
    public:
        explicit HttpHeadersTestSocket(const std::string& input)
            : _input(input)
            , _offset(0)
        {
        }

        std::ptrdiff_t recv(void* buffer, size_t length) override
        {
            size_t count = std::min(length, _input.size() - _offset);
            std::memcpy(buffer, _input.data() + _offset, count);
            _offset += count;
            return static_cast<std::ptrdiff_t>(count);
        }

    private:
        std::string _input;
        size_t _offset;
    };

    class RejectedHandshakeServer final : public ix::WebSocketServer
    {
    public:
        explicit RejectedHandshakeServer(const ix::HttpResponsePtr& response)
            : ix::WebSocketServer(ix::getFreePort(), "127.0.0.1")
            , _response(response)
        {
        }

        ~RejectedHandshakeServer() override
        {
            stop();
        }

    private:
        void handleConnection(std::unique_ptr<ix::Socket> socket,
                              std::shared_ptr<ix::ConnectionState> state) override
        {
            if (std::get<0>(ix::Http::parseRequest(socket, 2)))
                ix::Http::sendResponse(_response, socket);
            state->setTerminated();
        }

        ix::HttpResponsePtr _response;
    };
} // namespace

TEST_CASE("HTTP header line endings", "[http][headers]")
{
    const auto ending = GENERATE(std::string("\r\n"), std::string("\n"));
    CAPTURE(ending);

    std::unique_ptr<ix::Socket> socket(new HttpHeadersTestSocket(
        "X-Test: value  " + ending + "Location: http://localhost:8080/" + ending +
        "Empty:" + ending + ": Invalid: ignored" + ending + ending + "BODY"));
    auto result = ix::parseHttpHeaders(socket, [] { return false; });

    REQUIRE(result.first);
    REQUIRE(result.second.size() == 3);
    CHECK(result.second.at("x-test") == "value");
    CHECK(result.second.at("Location") == "http://localhost:8080/");
    CHECK(result.second.at("Empty").empty());

    char next;
    REQUIRE(socket->readByte(&next, [] { return false; }));
    CHECK(next == 'B');
}

TEST_CASE("Empty and mixed HTTP header blocks", "[http][headers]")
{
    std::string input;
    size_t expectedHeaders = 0;
    SECTION("Empty CRLF block")
    {
        input = "\r\nBODY";
    }
    SECTION("Empty LF block")
    {
        input = "\nBODY";
    }
    SECTION("Mixed line endings")
    {
        input = "First: one\r\nSecond: two\n\r\nBODY";
        expectedHeaders = 2;
    }

    std::unique_ptr<ix::Socket> socket(new HttpHeadersTestSocket(input));
    auto result = ix::parseHttpHeaders(socket, [] { return false; });
    REQUIRE(result.first);
    CHECK(result.second.size() == expectedHeaders);

    char next;
    REQUIRE(socket->readByte(&next, [] { return false; }));
    CHECK(next == 'B');
}

TEST_CASE("HTTP header line length limit", "[http][headers]")
{
    const auto ending = GENERATE(std::string("\r\n"), std::string("\n"));
    CAPTURE(ending);

    const std::string value(1023 - 3 - ending.size(), 'a');
    std::unique_ptr<ix::Socket> socket(new HttpHeadersTestSocket("X: " + value + ending + ending));
    auto result = ix::parseHttpHeaders(socket, [] { return false; });
    REQUIRE(result.first);
    CHECK(result.second.at("X") == value);

    socket.reset(new HttpHeadersTestSocket("X: " + value + "a" + ending + ending));
    CHECK_FALSE(ix::parseHttpHeaders(socket, [] { return false; }).first);

    socket.reset(new HttpHeadersTestSocket("X: " + std::string(1020, 'a') + "Injected: value" +
                                           ending + ending));
    result = ix::parseHttpHeaders(socket, [] { return false; });
    CHECK_FALSE(result.first);
    CHECK(result.second.count("Injected") == 0);
}

TEST_CASE("Incomplete or cancelled HTTP headers", "[http][headers]")
{
    std::string input;
    bool cancelled = false;
    SECTION("EOF")
    {
        input = "";
    }
    SECTION("Partial line")
    {
        input = "X: value";
    }
    SECTION("Missing header block terminator")
    {
        input = "X: value\n";
    }
    SECTION("Cancelled")
    {
        input = "X: value\n\n";
        cancelled = true;
    }

    std::unique_ptr<ix::Socket> socket(new HttpHeadersTestSocket(input));
    CHECK_FALSE(ix::parseHttpHeaders(socket, [cancelled] { return cancelled; }).first);
}

TEST_CASE("Rejected WebSocket handshakes expose response headers", "[websocket][http][headers]")
{
    const int status = GENERATE(401, 503);
    CAPTURE(status);
    const ix::WebSocketHttpHeaders headers = {
        {"X-Error-Reason", status == 401 ? "token_expired" : "server_busy"}, {"Retry-After", "60"}};
    RejectedHandshakeServer server(
        std::make_shared<ix::HttpResponse>(status,
                                           status == 401 ? "Unauthorized" : "Service Unavailable",
                                           ix::HttpErrorCode::Ok,
                                           headers,
                                           ""));
    REQUIRE(server.listen().first);
    server.start();

    std::promise<ix::WebSocketErrorInfo> error;
    auto result = error.get_future();
    ix::WebSocket client;
    client.disableAutomaticReconnection();
    client.setUrl("ws://127.0.0.1:" + std::to_string(server.getPort()) + "/");
    client.setOnMessageCallback(
        [&error](const ix::WebSocketMessagePtr& message)
        {
            if (message->type == ix::WebSocketMessageType::Error)
                error.set_value(message->errorInfo);
        });
    client.start();
    const auto ready = result.wait_for(std::chrono::seconds(2));
    client.stop();
    server.stop();

    REQUIRE(ready == std::future_status::ready);
    const auto info = result.get();
    CHECK(info.http_status == status);
    CHECK_FALSE(info.reason.empty());
    CHECK(info.headers.at("x-error-reason") == headers.at("X-Error-Reason"));
    CHECK(info.headers.at("retry-after") == "60");
}
