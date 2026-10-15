/*
 *  IXWebSocketHttpHeadersTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */

#include <algorithm>
#include <catch_amalgamated.hpp>
#include <cstring>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXWebSocketHttpHeaders.h>

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
