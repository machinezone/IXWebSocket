/*
 *  IXWebSocketHeartBeatTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <queue>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include "IXTest.h"

#include "catch.hpp"

using namespace ix;

namespace
{
    class WebSocketClient
    {
        public:
            WebSocketClient(int port);

            void subscribe(const std::string& channel);
            void start();
            void stop();
            bool isReady() const;

        private:
            ix::WebSocket _webSocket;
            int _port;
    };

    WebSocketClient::WebSocketClient(int port) 
        : _port(port)
    {
        ;
    }

    bool WebSocketClient::isReady() const
    {
        return _webSocket.getReadyState() == ix::WebSocket_ReadyState_Open;
    }

    void WebSocketClient::stop()
    {
        _webSocket.stop();
    }

    void WebSocketClient::start()
    {
        std::string url;
        {
            std::stringstream ss;
            ss << "ws://localhost:"
               << _port 
               << "/";

            url = ss.str();
        }

        _webSocket.setUrl(url);

        // The important bit for this test. 
        // Set a 1 second hearbeat ; if no traffic is present on the connection for 1 second
        // a ping message will be sent by the client.
        _webSocket.setHeartBeatPeriod(1);

        std::stringstream ss;
        log(std::string("Connecting to url: ") + url);

        _webSocket.setOnMessageCallback(
            [](ix::WebSocketMessageType messageType,
               const std::string& str,
               size_t wireSize,
               const ix::WebSocketErrorInfo& error,
               const ix::WebSocketOpenInfo& openInfo,
                   const ix::WebSocketCloseInfo& closeInfo)
            {
                std::stringstream ss;
                if (messageType == ix::WebSocket_MessageType_Open)
                {
                    log("client connected");
                }
                else if (messageType == ix::WebSocket_MessageType_Close)
                {
                    log("client disconnected");
                }
                else if (messageType == ix::WebSocket_MessageType_Error)
                {
                    ss << "Error ! " << error.reason;
                    log(ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Pong)
                {
                    ss << "Received pong message " << str;
                    log(ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Ping)
                {
                    ss << "Received ping message " << str;
                    log(ss.str());
                }
                else
                {
                    ss << "Invalid ix::WebSocketMessageType";
                    log(ss.str());
                }
            });

        _webSocket.start();
    }

    bool startServer(ix::WebSocketServer& server, std::atomic<int>& receivedPingMessages)
    {
        server.setOnConnectionCallback(
            [&server, &receivedPingMessages](std::shared_ptr<ix::WebSocket> webSocket)
            {
                webSocket->setOnMessageCallback(
                    [webSocket, &server, &receivedPingMessages](ix::WebSocketMessageType messageType,
                       const std::string& str,
                       size_t wireSize,
                       const ix::WebSocketErrorInfo& error,
                       const ix::WebSocketOpenInfo& openInfo,
                       const ix::WebSocketCloseInfo& closeInfo)
                    {
                        if (messageType == ix::WebSocket_MessageType_Open)
                        {
                            Logger() << "New connection";
                            Logger() << "Uri: " << openInfo.uri;
                            Logger() << "Headers:";
                            for (auto it : openInfo.headers)
                            {
                                Logger() << it.first << ": " << it.second;
                            }
                        }
                        else if (messageType == ix::WebSocket_MessageType_Close)
                        {
                            log("Closed connection");
                        }
                        else if (messageType == ix::WebSocket_MessageType_Ping)
                        {
                            log("Received a ping");
                            receivedPingMessages++;
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
            log(res.second);
            return false;
        }

        server.start();
        return true;
    }
}

TEST_CASE("Websocket_heartbeat", "[heartbeat]")
{
    SECTION("Make sure that ping messages are sent during heartbeat.")
    {
        ix::setupWebSocketTrafficTrackerCallback();

        int port = 8092;
        ix::WebSocketServer server(port);
        std::atomic<int> serverReceivedPingMessages(0);
        REQUIRE(startServer(server, serverReceivedPingMessages));

        std::string session = ix::generateSessionId();
        WebSocketClient webSocketClientA(port);
        WebSocketClient webSocketClientB(port);

        webSocketClientA.start();
        webSocketClientB.start();

        // Wait for all chat instance to be ready
        while (true)
        {
            if (webSocketClientA.isReady() && webSocketClientB.isReady()) break;
            ix::msleep(10);
        }

        REQUIRE(server.getClients().size() == 2);

        ix::msleep(3000);

        webSocketClientA.stop();
        webSocketClientB.stop();

        REQUIRE(serverReceivedPingMessages >= 4);

        // Give us 500ms for the server to notice that clients went away
        ix::msleep(500);
        REQUIRE(server.getClients().size() == 0);

        ix::reportWebSocketTraffic();
    }
}
