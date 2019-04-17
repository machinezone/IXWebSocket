/*
 *  IXWebSocketHeartBeatNoResponseAutoDisconnectTest.cpp
 *  Author: Alexandre Konieczny
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
            bool isClosed() const;
            void sendMessage(const std::string& text);
            int getReceivedPongMessages();
            bool closedDueToPingTimeout();

        private:
            ix::WebSocket _webSocket;
            int _port;
            std::atomic<int> _receivedPongMessages;
            std::atomic<bool> _closedDueToPingTimeout;
    };

    WebSocketClient::WebSocketClient(int port)
        : _port(port),
         _receivedPongMessages(0),
         _closedDueToPingTimeout(false)
    {
        ;
    }

    bool WebSocketClient::isReady() const
    {
        return _webSocket.getReadyState() == ix::WebSocket_ReadyState_Open;
    }

    bool WebSocketClient::isClosed() const
    {
        return _webSocket.getReadyState() == ix::WebSocket_ReadyState_Closed;
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
        // Set a 1 second ping interval, and 2 second ping timeout
        _webSocket.setPingInterval(1);
        _webSocket.setPingTimeout(2); //

        std::stringstream ss;
        log(std::string("Connecting to url: ") + url);

        _webSocket.setOnMessageCallback(
            [this](ix::WebSocketMessageType messageType,
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

                    _webSocket.disableAutomaticReconnection();
                }
                else if (messageType == ix::WebSocket_MessageType_Close)
                {
                    log("client disconnected");

                    if (closeInfo.code == 1011)
                    {
                        _closedDueToPingTimeout = true;
                    }

                    _webSocket.disableAutomaticReconnection();
        
                }
                else if (messageType == ix::WebSocket_MessageType_Error)
                {
                    ss << "Error ! " << error.reason;
                    log(ss.str());

                    _webSocket.disableAutomaticReconnection();
                }
                else if (messageType == ix::WebSocket_MessageType_Pong)
                {
                    _receivedPongMessages++;

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

    int WebSocketClient::getReceivedPongMessages()
    {
        return _receivedPongMessages;
    }

    bool WebSocketClient::closedDueToPingTimeout()
    {
        return _closedDueToPingTimeout;
    }

    bool startServer(ix::WebSocketServer& server, std::atomic<int>& receivedPingMessages, bool enablePong)
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

        if (!enablePong)
        {
            // USE this to prevent a pong answer, so the ping timeout is raised on client
            server.disablePong();
        }

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

TEST_CASE("Websocket_ping_no_timeout", "[setPingTimeout]")
{
    SECTION("Make sure that ping messages have a response (PONG).")
    {
        ix::setupWebSocketTrafficTrackerCallback();

        int port = getFreePort();
        ix::WebSocketServer server(port);  
        std::atomic<int> serverReceivedPingMessages(0);
        REQUIRE(startServer(server, serverReceivedPingMessages, true)); // true as Pong is enable on Server

        std::string session = ix::generateSessionId();
        WebSocketClient webSocketClient(port);

        webSocketClient.start();

        // Wait for all chat instance to be ready
        while (true)
        {
            if (webSocketClient.isReady()) break;
            ix::msleep(10);
        }

        REQUIRE(server.getClients().size() == 1);

        ix::msleep(1100);

        // Here we test ping timeout, no timeout
        REQUIRE(serverReceivedPingMessages == 1);
        REQUIRE(webSocketClient.getReceivedPongMessages() == 1);

        ix::msleep(1000);

        // Here we test ping timeout, no timeout
        REQUIRE(serverReceivedPingMessages == 2);
        REQUIRE(webSocketClient.getReceivedPongMessages() == 2);

        webSocketClient.stop();

        // Give us 500ms for the server to notice that clients went away
        ix::msleep(500);
        REQUIRE(server.getClients().size() == 0);

        // Ensure client close was not by ping timeout
        REQUIRE(webSocketClient.closedDueToPingTimeout() == false);

        ix::reportWebSocketTraffic();
    }
}

TEST_CASE("Websocket_ping_timeout", "[setPingTimeout]")
{
    SECTION("Make sure that ping messages don't have responses (no PONG).")
    {
        ix::setupWebSocketTrafficTrackerCallback();

        int port = getFreePort();
        ix::WebSocketServer server(port);  
        std::atomic<int> serverReceivedPingMessages(0);
        REQUIRE(startServer(server, serverReceivedPingMessages, false)); // false as Pong is DISABLED on Server

        std::string session = ix::generateSessionId();
        WebSocketClient webSocketClient(port);

        webSocketClient.start();

        // Wait for all chat instance to be ready
        while (true)
        {
            if (webSocketClient.isReady()) break;
            ix::msleep(10);
        }

        REQUIRE(server.getClients().size() == 1);

        ix::msleep(1100);

        // Here we test ping timeout, no timeout yet
        REQUIRE(serverReceivedPingMessages == 1);
        REQUIRE(webSocketClient.getReceivedPongMessages() == 0);

        ix::msleep(1000);

        // Here we test ping timeout, timeout
        REQUIRE(serverReceivedPingMessages == 1);
        REQUIRE(webSocketClient.getReceivedPongMessages() == 0);
        // Ensure client close was not by ping timeout
        REQUIRE(webSocketClient.isClosed() == true);
        REQUIRE(webSocketClient.closedDueToPingTimeout() == true);

        webSocketClient.stop();

        REQUIRE(server.getClients().size() == 0);

        ix::reportWebSocketTraffic();
    }
}
