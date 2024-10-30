/*
 *  IXWebSocketServerTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include "catch.hpp"
#include <iostream>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXSocketFactory.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>

using namespace ix;

bool startServer(ix::WebSocketServer& server, std::string& subProtocols)
{
    server.setOnClientMessageCallback(
        [&server, &subProtocols](std::shared_ptr<ConnectionState> connectionState,
                                 WebSocket& webSocket,
                                 const ix::WebSocketMessagePtr& msg) {
            auto remoteIp = connectionState->getRemoteIp();
            if (msg->type == ix::WebSocketMessageType::Open)
            {
                TLogger() << "New connection";
                TLogger() << "remote ip: " << remoteIp;
                TLogger() << "id: " << connectionState->getId();
                TLogger() << "Uri: " << msg->openInfo.uri;
                TLogger() << "Headers:";
                for (auto it : msg->openInfo.headers)
                {
                    TLogger() << it.first << ": " << it.second;
                }

                subProtocols = msg->openInfo.headers["Sec-WebSocket-Protocol"];
            }
            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                log("Closed connection");
            }
            else if (msg->type == ix::WebSocketMessageType::Message)
            {
                for (auto&& client : server.getClients())
                {
                    if (client.get() != &webSocket)
                    {
                        client->sendBinary(msg->str);
                    }
                }
            }
        });

    auto res = server.listen();
    if (!res.first)
    {
        log(res.second);
        return false;
    }

    server.start();
    return true;
}

void runTest(int port, const ix::WebSocketHttpHeaders & headers)
{
        ix::WebSocketServer server(port);

        std::string subProtocols;
        startServer(server, subProtocols);

        std::atomic<bool> connected(false);
        ix::WebSocket webSocket;

        if(!headers.empty()){
            webSocket.setExtraHeaders(headers);
        }

        webSocket.setOnMessageCallback([&connected](const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Open)
            {
                connected = true;
                log("Client connected");
            }
        });

        webSocket.addSubProtocol("json");
        webSocket.addSubProtocol("msgpack");

        std::string url;
        std::stringstream ss;
        ss << "ws://127.0.0.1:" << port;
        url = ss.str();

        webSocket.setUrl(url);
        webSocket.start();

        // Give us 3 seconds to connect
        int attempts = 0;
        while (!connected)
        {
            REQUIRE(attempts++ < 300);
            ix::msleep(10);
        }

        webSocket.stop();
        server.stop();

        REQUIRE(subProtocols == "json,msgpack");
    }
    

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
