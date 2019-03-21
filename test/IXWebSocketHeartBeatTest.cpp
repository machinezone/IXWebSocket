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
            void sendMessage(const std::string& text);

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
        // Set a 1 second heartbeat ; if no traffic is present on the connection for 1 second
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
                else if (messageType == ix::WebSocket_MessageType_Message)
                {
                    ss << "Received message " << str;
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

    void WebSocketClient::sendMessage(const std::string& text)
    {
        _webSocket.send(text);
    }

    bool startServer(ix::WebSocketServer& server, std::atomic<int>& receivedPingMessages)
    {
        // A dev/null server
        server.setOnConnectionCallback(
            [&server, &receivedPingMessages](std::shared_ptr<ix::WebSocket> webSocket,
                                             std::shared_ptr<ConnectionState> connectionState)
            {
                webSocket->setOnMessageCallback(
                    [webSocket, connectionState, &server, &receivedPingMessages](ix::WebSocketMessageType messageType,
                       const std::string& str,
                       size_t wireSize,
                       const ix::WebSocketErrorInfo& error,
                       const ix::WebSocketOpenInfo& openInfo,
                       const ix::WebSocketCloseInfo& closeInfo)
                    {
                        if (messageType == ix::WebSocket_MessageType_Open)
                        {
                            Logger() << "New server connection";
                            Logger() << "id: " << connectionState->getId();
                            Logger() << "Uri: " << openInfo.uri;
                            Logger() << "Headers:";
                            for (auto it : openInfo.headers)
                            {
                                Logger() << it.first << ": " << it.second;
                            }
                        }
                        else if (messageType == ix::WebSocket_MessageType_Close)
                        {
                            log("Server closed connection");
                        }
                        else if (messageType == ix::WebSocket_MessageType_Ping)
                        {
                            log("Server received a ping");
                            receivedPingMessages++;
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

        int port = getFreePort();
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

        ix::msleep(900);
        webSocketClientB.sendMessage("hello world");
        ix::msleep(900);
        webSocketClientB.sendMessage("hello world");
        ix::msleep(900);

        webSocketClientA.stop();
        webSocketClientB.stop();


        // Here we test heart beat period exceeded for clientA
        // but it should not be exceeded for clientB which has sent data.
        // -> expected ping messages == 2, but add a small buffer to make this more reliable.
        REQUIRE(serverReceivedPingMessages >= 2);
        REQUIRE(serverReceivedPingMessages <= 4);

        // Give us 500ms for the server to notice that clients went away
        ix::msleep(500);
        REQUIRE(server.getClients().size() == 0);

        ix::reportWebSocketTraffic();
    }
}
