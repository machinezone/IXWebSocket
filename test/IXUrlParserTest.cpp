/*
 *  IXSocketTest.cpp
 *  Author: Korchynskyi Dmytro
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <ixwebsocket/IXUrlParser.h>
#include <string.h>
#include <vector>

using namespace ix;

namespace ix
{
    TEST_CASE("urlParser", "[urlParser]")
    {
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
                "ws://user@@127.0.0.1/",     // multiple @ separators in authority
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
