/*
 *  IXSocketTest.cpp
 *  Author: Korchynskyi Dmytro
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include <catch_amalgamated.hpp>
#include <clocale>
#include <iostream>
#include <ixwebsocket/IXUrlParser.h>
#include <string.h>
#include <vector>

using namespace ix;

namespace ix
{
    TEST_CASE("urlParser", "[urlParser]")
    {
        SECTION("SOCKS schemes allow digits after the initial letter")
        {
            std::string protocol, host, path, query;
            int port;
            auto scheme = GENERATE(std::string("socks5"), std::string("socks5h"));
            REQUIRE(
                UrlParser::parse(scheme + "://127.0.0.1:1080", protocol, host, path, query, port));
            CHECK(protocol == scheme);
            CHECK(host == "127.0.0.1");
            CHECK(port == 1080);
        }
        SECTION("Reject invalid scheme starts and explicit out-of-range ports")
        {
            std::string protocol, host, path, query;
            int port;
            auto url = GENERATE("://localhost",
                                "5socks://localhost",
                                "+socks://localhost",
                                "socks5://localhost:0",
                                "socks5://localhost:65536",
                                "socks5://localhost:99999999999999999999999999");
            CHECK_FALSE(UrlParser::parse(url, protocol, host, path, query, port));
        }
        SECTION("http://google.com")
        {
            std::string url = "http://google.com";
            std::string protocol, host, path, query;
            int port;
            bool res;

            res = UrlParser::parse(url, protocol, host, path, query, port);

            REQUIRE(res);
            REQUIRE(protocol == "http");
            REQUIRE(host == "google.com");
            REQUIRE(path == "/");
            REQUIRE(query == "");
            REQUIRE(port == 80); // default port for http
        }

        SECTION("https://google.com")
        {
            std::string url = "https://google.com";
            std::string protocol, host, path, query;
            int port;
            bool res;

            res = UrlParser::parse(url, protocol, host, path, query, port);

            REQUIRE(res);
            REQUIRE(protocol == "https");
            REQUIRE(host == "google.com");
            REQUIRE(path == "/");
            REQUIRE(query == "");
            REQUIRE(port == 443); // default port for https
        }

        SECTION("ws://google.com")
        {
            std::string url = "ws://google.com";
            std::string protocol, host, path, query;
            int port;
            bool res;

            res = UrlParser::parse(url, protocol, host, path, query, port);

            REQUIRE(res);
            REQUIRE(protocol == "ws");
            REQUIRE(host == "google.com");
            REQUIRE(path == "/");
            REQUIRE(query == "");
            REQUIRE(port == 80); // default port for ws
        }

        SECTION("wss://google.com/ws?arg=value")
        {
            std::string url = "wss://google.com/ws?arg=value&arg2=value2";
            std::string protocol, host, path, query;
            int port;
            bool res;

            res = UrlParser::parse(url, protocol, host, path, query, port);

            REQUIRE(res);
            REQUIRE(protocol == "wss");
            REQUIRE(host == "google.com");
            REQUIRE(path == "/ws?arg=value&arg2=value2");
            REQUIRE(query == "arg=value&arg2=value2");
            REQUIRE(port == 443); // default port for wss
        }

        SECTION("wss://google.com/?arg=value")
        {
            std::string url = "wss://google.com/?arg=value&arg2=value2";
            std::string protocol, host, path, query;
            int port;
            bool res;

            res = UrlParser::parse(url, protocol, host, path, query, port);

            REQUIRE(res);
            REQUIRE(protocol == "wss");
            REQUIRE(host == "google.com");
            REQUIRE(path == "/?arg=value&arg2=value2");
            REQUIRE(query == "arg=value&arg2=value2");
            REQUIRE(port == 443); // default port for wss
        }

        SECTION("wss://google.com?arg=value")
        {
            std::string url = "wss://google.com?arg=value&arg2=value2";
            std::string protocol, host, path, query;
            int port;
            bool res;

            res = UrlParser::parse(url, protocol, host, path, query, port);

            REQUIRE(res);
            REQUIRE(protocol == "wss");
            REQUIRE(host == "google.com");
            REQUIRE(path == "/?arg=value&arg2=value2");
            REQUIRE(query == "arg=value&arg2=value2");
            REQUIRE(port == 443); // default port for wss
        }

        SECTION("real test")
        {
            std::string url =
                "ws://127.0.0.1:7350/"
                "ws?token=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
                "eyJleHAiOjE1NTcxNzAwNzIsInVpZCI6ImMwZmZjOGE1LTk4OTktNDAwYi1hNGU5LTJjNWM3NjFmNWQxZi"
                "IsInVzbiI6InN2YmhOdlNJSmEifQ.5L8BUbpTA4XAHlSrdwhIVlrlIpRtjExepim7Yh5eEO4&status="
                "true&format=protobuf";
            std::string protocol, host, path, query;
            int port;
            bool res;

            res = UrlParser::parse(url, protocol, host, path, query, port);

            REQUIRE(res);
            REQUIRE(protocol == "ws");
            REQUIRE(host == "127.0.0.1");
            REQUIRE(path ==
                    "/ws?token=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
                    "eyJleHAiOjE1NTcxNzAwNzIsInVpZCI6ImMwZmZjOGE1LTk4OTktNDAwYi1hNGU5LTJjNWM3NjFmNW"
                    "QxZiIsInVzbiI6InN2YmhOdlNJSmEifQ.5L8BUbpTA4XAHlSrdwhIVlrlIpRtjExepim7Yh5eEO4&"
                    "status=true&format=protobuf");
            REQUIRE(query ==
                    "token=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
                    "eyJleHAiOjE1NTcxNzAwNzIsInVpZCI6ImMwZmZjOGE1LTk4OTktNDAwYi1hNGU5LTJjNWM3NjFmNW"
                    "QxZiIsInVzbiI6InN2YmhOdlNJSmEifQ.5L8BUbpTA4XAHlSrdwhIVlrlIpRtjExepim7Yh5eEO4&"
                    "status=true&format=protobuf");
            REQUIRE(port == 7350);
        }

        SECTION("reject malformed IPv4 authority")
        {
            std::vector<std::string> malformedUrls = {
                "ws://:9001/",             // empty host
                "ws://127.0.0.1::9001/",   // malformed port separator
                "ws://127.0.0.1:abc/",     // non-numeric port
                "ws://127.0.0.1:9001abc/", // non-numeric port suffix
                "ws://127.0.0.1:/",        // missing digits after ':'
            };

            for (const auto& url : malformedUrls)
            {
                std::string protocol, host, path, query;
                int port = -1;

                bool res = UrlParser::parse(url, protocol, host, path, query, port);
                CHECK_FALSE(res);
            }
        }

        SECTION("reject malformed bracketed IPv6 authority")
        {
            std::vector<std::string> malformedUrls = {
                "ws://[::1/",        // missing closing bracket
                "ws://[::1]x",       // invalid token after closing bracket
                "ws://[]:9001/",     // empty IPv6 host
                "ws://[::1]:abc/",   // non-numeric port
                "ws://[::1]:9001x/", // non-numeric port suffix
                "ws://[::1]:/",      // missing digits after ':'
                "ws://::1]/",        // unmatched closing bracket in authority
                "ws://[::1/:9001",   // missing closing bracket before path
                "ws://::1/",         // unbracketed IPv6 authority
            };

            for (const auto& url : malformedUrls)
            {
                std::string protocol, host, path, query;
                int port = -1;

                bool res = UrlParser::parse(url, protocol, host, path, query, port);
                CHECK_FALSE(res);
            }
        }

        SECTION("reject malformed userinfo")
        {
            std::vector<std::string> malformedUrls = {
                "ws://user@@127.0.0.1/",      // multiple @ separators in authority
                "ws://user:pass@@[::1]:9001/" // multiple @ separators in authority
            };

            for (const auto& url : malformedUrls)
            {
                std::string protocol, host, path, query;
                int port = -1;

                bool res = UrlParser::parse(url, protocol, host, path, query, port);
                CHECK_FALSE(res);
            }
        }
    }

} // namespace ix

TEST_CASE("URL components preserve authority boundaries and empty queries", "[url][components]")
{
    const std::string url = GENERATE(std::string("http://example.com#section"),
                                     std::string("http://example.com:8080#section"),
                                     std::string("http://[::1]#section"),
                                     std::string("http://example.com/path?"),
                                     std::string("http://example.com?"));
    std::string protocol, host, path, query;
    int port = 0;
    REQUIRE(ix::UrlParser::parse(url, protocol, host, path, query, port));
    CHECK(host == (url.find("[::1]") != std::string::npos ? "::1" : "example.com"));
    CHECK(port == (url.find("8080") != std::string::npos ? 8080 : 80));
    CHECK(path == (url.find("/path") != std::string::npos ? "/path?"
                   : url.back() == '?'                    ? "/?"
                                                          : "/"));
}

TEST_CASE("Explicit invalid URL ports do not fall back to a default", "[url][components]")
{
    const std::string portText = GENERATE(std::string("0"),
                                          std::string("65536"),
                                          std::string("4294967376"),
                                          std::string("99999999999999999999999999999999"));
    const std::string hostText = GENERATE(std::string("example.com"), std::string("[::1]"));
    std::string protocol, host, path, query;
    int port = 0;
    CHECK_FALSE(ix::UrlParser::parse(
        "http://" + hostText + ":" + portText + "/", protocol, host, path, query, port));
}

TEST_CASE("URL schemes follow the ASCII scheme grammar", "[url][components]")
{
    const std::string scheme = GENERATE(std::string(""),
                                        std::string("+http"),
                                        std::string(".http"),
                                        std::string("1http"),
                                        std::string("h\xe9ttp"));
    std::string protocol, host, path, query;
    int port = 0;
    CHECK_FALSE(
        ix::UrlParser::parse(scheme + "://example.com/", protocol, host, path, query, port));
    REQUIRE(ix::UrlParser::parse("SOCKS5://example.com:1080/", protocol, host, path, query, port));
    CHECK(protocol == "socks5");
    CHECK(port == 1080);
}

TEST_CASE("URL schemes stay ASCII in single byte locales", "[url][components]")
{
    const char* current = std::setlocale(LC_CTYPE, nullptr);
    const std::string saved = current ? current : "C";

    struct RestoreLocale
    {
        std::string name;

        ~RestoreLocale()
        {
            std::setlocale(LC_CTYPE, name.c_str());
        }
    } restore {saved};

    if (!std::setlocale(LC_CTYPE, "en_US.ISO8859-1"))
    {
        SKIP("Single-byte test locale is unavailable");
    }
    std::string protocol, host, path, query;
    int port = 0;
    CHECK_FALSE(ix::UrlParser::parse("h\xe9ttp://example.com/", protocol, host, path, query, port));
    REQUIRE(ix::UrlParser::parse("HTTPS://example.com/", protocol, host, path, query, port));
    CHECK(protocol == "https");
}

TEST_CASE("URLs reject raw whitespace and control characters", "[url][components]")
{
    const char character = GENERATE(' ', '\t', '\r', '\n', '\0', '\x7f');
    const std::string prefix = GENERATE(std::string("http://exam"),
                                        std::string("http://example.com/path"),
                                        std::string("http://example.com/?query="));
    std::string protocol, host, path, query;
    int port = 0;
    CHECK_FALSE(
        ix::UrlParser::parse(prefix + character + "text", protocol, host, path, query, port));
    CHECK(ix::UrlParser::parse("http://example.com/a%20b", protocol, host, path, query, port));
}

TEST_CASE("URL origins compare scheme, hostname and effective port", "[url][origin]")
{
    struct Example
    {
        const char* first;
        const char* second;
        bool same;
    };

    const Example examples[] = {
        {"HTTP://EXAMPLE.com/a", "http://example.COM:80/b?query#fragment", true},
        {"https://example.com/a", "https://example.com:443/b", true},
        {"ws://example.com", "ws://example.com:80/", true},
        {"wss://[FE80::ABCD]", "wss://[fe80::abcd]:443/", true},
        {"https://example.com", "http://example.com:443", false},
        {"http://example.com", "http://other.example.com", false},
        {"http://example.com", "http://example.com:8080", false},
        {"ws://example.com", "http://example.com", false},
        {"http://example.com", "http://example.com:0", false},
        {"invalid", "invalid", false}};
    for (const auto& example : examples)
    {
        CAPTURE(example.first, example.second);
        CHECK(ix::UrlParser::isSameOrigin(example.first, example.second) == example.same);
        CHECK(ix::UrlParser::isSameOrigin(example.second, example.first) == example.same);
    }
}
