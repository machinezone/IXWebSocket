/*
 *  IXWebSocketHostTest.cpp
 *  Copyright (c) 2019-2026 Machine Zone, Inc. All rights reserved.
 */

#include "IXTest.h"
#include <atomic>
#include <catch_amalgamated.hpp>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>

using namespace ix;

namespace
{
    WebSocketHttpHeaders runTest(WebSocket& webSocket, int port)
    {
        WebSocketHttpHeaders receivedHeaders;
        WebSocketServer server(port, "127.0.0.1");
        server.setOnClientMessageCallback(
            [&](std::shared_ptr<ConnectionState>, WebSocket&, const WebSocketMessagePtr& msg)
            {
                if (msg->type == WebSocketMessageType::Open)
                    receivedHeaders = msg->openInfo.headers;
            });
        REQUIRE(server.listen().first);
        server.start();

        std::atomic<bool> connected {false};
        webSocket.setOnMessageCallback(
            [&](const WebSocketMessagePtr& msg)
            {
                if (msg->type == WebSocketMessageType::Open) connected = true;
            });
        webSocket.disableAutomaticReconnection();
        if (webSocket.getSubProtocols().empty())
        {
            webSocket.addSubProtocol("json");
            webSocket.addSubProtocol("msgpack");
        }
        webSocket.setUrl("ws://127.0.0.1:" + std::to_string(port));
        webSocket.start();
        for (int attempt = 0; attempt < 300 && !connected; ++attempt)
            ix::msleep(10);

        // Join both sides before reading headers captured by the server callback.
        webSocket.stop();
        server.stop();
        webSocket.setOnMessageCallback(nullptr);
        REQUIRE(connected);
        REQUIRE(receivedHeaders.at("Sec-WebSocket-Protocol") == "json,msgpack");
        return receivedHeaders;
    }

    void runTest(int port, const WebSocketHttpHeaders& headers)
    {
        WebSocket webSocket;
        webSocket.setExtraHeaders(headers);
        runTest(webSocket, port);
    }
} // namespace

TEST_CASE("host", "[websocket_host]")
{
    SECTION("Connect to the server, standard host header")
    {
        int port = getFreePort();
        runTest(port, {});
    }

    SECTION("Connect to the server, specific host a.b.c.d:port header")
    {
        int port = getFreePort();
        runTest(port, {{"Host", "127.0.0.1:" + std::to_string(port)}});
    }

    SECTION("Connect to the server, specific host localhost:port header")
    {
        int port = getFreePort();
        runTest(port, {{"Host", "localhost:" + std::to_string(port)}});
    }

    SECTION("Connect to the server, specific host a.b.c.d header")
    {
        int port = getFreePort();
        runTest(port, {{"Host", "127.0.0.1"}});
    }

    SECTION("Connect to the server, specific host localhost header")
    {
        int port = getFreePort();
        runTest(port, {{"Host", "localhost"}});
    }
}

TEST_CASE("Optional default handshake headers can be disabled individually", "[websocket_host]")
{
    const bool disableUserAgent = GENERATE(false, true);
    const bool disableOrigin = GENERATE(false, true);
    CAPTURE(disableUserAgent, disableOrigin);
    std::vector<std::string> disabled;
    if (disableUserAgent) disabled.push_back("User-Agent");
    if (disableOrigin) disabled.push_back("Origin");
    WebSocket client;
    client.setDisabledDefaultHeaders(disabled);
    const auto received = runTest(client, getFreePort());
    CHECK(received.count("User-Agent") == (disableUserAgent ? 0 : 1));
    CHECK(received.count("Origin") == (disableOrigin ? 0 : 1));
}

TEST_CASE("Disabled default header names are case insensitive", "[websocket_host]")
{
    const auto names = GENERATE(std::vector<std::string> {"user-agent", "origin"},
                                std::vector<std::string> {"uSeR-aGeNt", "ORIGIN", "User-Agent"});
    WebSocket client;
    client.setDisabledDefaultHeaders(names);
    const auto received = runTest(client, getFreePort());
    CHECK(received.count("User-Agent") == 0);
    CHECK(received.count("Origin") == 0);
}

TEST_CASE("Explicit extra headers take precedence over disabled defaults", "[websocket_host]")
{
    const bool empty = GENERATE(false, true);
    CAPTURE(empty);
    const std::string agent = empty ? "" : "custom-agent";
    const std::string origin = empty ? "" : "https://example.org";
    WebSocket client;
    client.setDisabledDefaultHeaders({"USER-AGENT", "origin", "X-Custom"});
    client.setExtraHeaders({{"user-agent", agent}, {"ORIGIN", origin}, {"X-Custom", ""}});
    const auto received = runTest(client, getFreePort());
    REQUIRE(received.count("User-Agent") == 1);
    REQUIRE(received.count("Origin") == 1);
    REQUIRE(received.count("X-Custom") == 1);
    CHECK(received.at("User-Agent") == agent);
    CHECK(received.at("Origin") == origin);
    CHECK(received.at("X-Custom").empty());
}

TEST_CASE("Replacing or clearing disabled defaults takes effect on the next connection",
          "[websocket_host]")
{
    WebSocket client;
    client.setDisabledDefaultHeaders({"User-Agent", "Origin"});
    auto received = runTest(client, getFreePort());
    CHECK(received.count("User-Agent") == 0);
    CHECK(received.count("Origin") == 0);

    client.setDisabledDefaultHeaders({"User-Agent"});
    received = runTest(client, getFreePort());
    CHECK(received.count("User-Agent") == 0);
    CHECK(received.count("Origin") == 1);

    client.setDisabledDefaultHeaders({});
    received = runTest(client, getFreePort());
    REQUIRE(received.count("User-Agent") == 1);
    REQUIRE(received.count("Origin") == 1);
    CHECK_FALSE(received.at("User-Agent").empty());
    CHECK_FALSE(received.at("Origin").empty());
}

TEST_CASE("Required handshake headers cannot be disabled", "[websocket_host]")
{
    WebSocket client;
    client.setDisabledDefaultHeaders({"host",
                                      "Upgrade",
                                      "Connection",
                                      "Sec-WebSocket-Version",
                                      "Sec-WebSocket-Key",
                                      "X-Unknown"});
    const int port = getFreePort();
    const auto received = runTest(client, port);
    CHECK(received.at("Host") == "127.0.0.1:" + std::to_string(port));
    CHECK(received.at("Upgrade") == "websocket");
    CHECK(received.at("Connection") == "Upgrade");
    CHECK(received.at("Sec-WebSocket-Version") == "13");
    CHECK_FALSE(received.at("Sec-WebSocket-Key").empty());
    CHECK(received.count("User-Agent") == 1);
    CHECK(received.count("Origin") == 1);
}
