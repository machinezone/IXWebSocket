/*
 *  IXTestWebSocketServer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#include <iostream>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include "IXTest.h"

#include "catch.hpp"

using namespace ix;

namespace ix
{
    bool startServer(ix::WebSocketServer& server)
    {
        server.setOnConnectionCallback(
            [&server](std::shared_ptr<ix::WebSocket> webSocket)
            {
                webSocket->setOnMessageCallback(
                    [webSocket, &server](ix::WebSocketMessageType messageType,
                       const std::string& str,
                       size_t wireSize,
                       const ix::WebSocketErrorInfo& error,
                       const ix::WebSocketCloseInfo& closeInfo,
                       const ix::WebSocketHttpHeaders& headers)
                    {
                        if (messageType == ix::WebSocket_MessageType_Open)
                        {
                            std::cerr << "New connection" << std::endl;
                            std::cerr << "Headers:" << std::endl;
                            for (auto it : headers)
                            {
                                std::cerr << it.first << ": " << it.second << std::endl;
                            }
                        }
                        else if (messageType == ix::WebSocket_MessageType_Close)
                        {
                            std::cerr << "Closed connection" << std::endl;
                        }
                        else if (messageType == ix::WebSocket_MessageType_Message)
                        {
                            for (auto&& client : server.getClients())
                            {
                                if (client != webSocket)
                                {
                                    client->send(str);
                                }
                            }
                        }
                    }
                );
            }
        );

        auto res = server.listen();
        if (!res.first)
        {
            std::cerr << res.second << std::endl;
            return false;
        }

        server.start();
        return true;
    }
}

TEST_CASE("Websocket_server", "[websocket_server]")
{
    SECTION("Connect to the server, do not send anything. Should timeout and return 400")
    {
        int port = 8091;
        ix::WebSocketServer server(port);
        REQUIRE(startServer(server));

        Socket socket;
        std::string host("localhost");
        std::string errMsg;
        auto isCancellationRequested = []() -> bool
        {
            return false;
        };
        bool success = socket.connect(host, port, errMsg, isCancellationRequested);
        REQUIRE(success);

        auto lineResult = socket.readLine(isCancellationRequested);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        int status = -1;
        REQUIRE(sscanf(line.c_str(), "HTTP/1.1 %d", &status) == 1);
        REQUIRE(status == 400);

        // FIXME: explicitely set a client timeout larger than the server one (3)

        // Give us 500ms for the server to notice that clients went away
        ix::msleep(500);
        server.stop();
        REQUIRE(server.getClients().size() == 0);
    }

    SECTION("Connect to the server. Send GET request without header. Should return 400")
    {
        int port = 8092;
        ix::WebSocketServer server(port);
        REQUIRE(startServer(server));

        Socket socket;
        std::string host("localhost");
        std::string errMsg;
        auto isCancellationRequested = []() -> bool
        {
            return false;
        };
        bool success = socket.connect(host, port, errMsg, isCancellationRequested);
        REQUIRE(success);

        std::cout << "writeBytes" << std::endl;
        socket.writeBytes("GET /\r\n", isCancellationRequested);

        auto lineResult = socket.readLine(isCancellationRequested);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        int status = -1;
        REQUIRE(sscanf(line.c_str(), "HTTP/1.1 %d", &status) == 1);
        REQUIRE(status == 400);

        // FIXME: explicitely set a client timeout larger than the server one (3)

        // Give us 500ms for the server to notice that clients went away
        ix::msleep(500);
        server.stop();
        REQUIRE(server.getClients().size() == 0);
    }

    SECTION("Connect to the server. Send GET request with correct header")
    {
        int port = 8093;
        ix::WebSocketServer server(port);
        REQUIRE(startServer(server));

        Socket socket;
        std::string host("localhost");
        std::string errMsg;
        auto isCancellationRequested = []() -> bool
        {
            return false;
        };
        bool success = socket.connect(host, port, errMsg, isCancellationRequested);
        REQUIRE(success);

        socket.writeBytes("GET /\r\nSec-WebSocket-Key: foobar\r\n\r\n", isCancellationRequested);
        auto lineResult = socket.readLine(isCancellationRequested);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        int status = -1;
        REQUIRE(sscanf(line.c_str(), "HTTP/1.1 %d", &status) == 1);
        REQUIRE(status == 101);

        // Give us 500ms for the server to notice that clients went away
        ix::msleep(500);

        server.stop();
        REQUIRE(server.getClients().size() == 0);
    }
}
