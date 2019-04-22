/*
 *  IXWebSocketPingTest.cpp
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
            WebSocketClient(int port, bool useHeartBeatMethod);

            void subscribe(const std::string& channel);
            void start();
            void stop();
            bool isReady() const;
            void sendMessage(const std::string& text);

        private:
            ix::WebSocket _webSocket;
            int _port;
            bool _useHeartBeatMethod;
    };

    WebSocketClient::WebSocketClient(int port, bool useHeartBeatMethod)
        : _port(port),
          _useHeartBeatMethod(useHeartBeatMethod)
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
        // Set a 1 second heartbeat with the setter method to test
        if (_useHeartBeatMethod)
            _webSocket.setHeartBeatPeriod(1);
        else
            _webSocket.setPingInterval(1);

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

TEST_CASE("Websocket_ping_no_data_sent_setHeartBeatPeriod", "[setHeartBeatPeriod]")
{
    SECTION("Make sure that ping messages are sent when no other data are sent.")
    {
        ix::setupWebSocketTrafficTrackerCallback();

        int port = getFreePort();
        ix::WebSocketServer server(port);
        std::atomic<int> serverReceivedPingMessages(0);
        REQUIRE(startServer(server, serverReceivedPingMessages));

        std::string session = ix::generateSessionId();
        bool useSetHeartBeatPeriodMethod = true;
        WebSocketClient webSocketClient(port, useSetHeartBeatPeriodMethod);

        webSocketClient.start();

        // Wait for all chat instance to be ready
        while (true)
        {
            if (webSocketClient.isReady()) break;
            ix::msleep(10);
        }

        REQUIRE(server.getClients().size() == 1);

        ix::msleep(1900);

        webSocketClient.stop();


        // Here we test ping interval
        // -> expected ping messages == 1 as 1900 seconds, 1 ping sent every second
        REQUIRE(serverReceivedPingMessages == 1);

        // Give us 500ms for the server to notice that clients went away
        ix::msleep(500);
        REQUIRE(server.getClients().size() == 0);

        ix::reportWebSocketTraffic();
    }
}

TEST_CASE("Websocket_ping_data_sent_setHeartBeatPeriod", "[setHeartBeatPeriod]")
{
    SECTION("Make sure that ping messages are sent, even if other messages are sent")
    {
        ix::setupWebSocketTrafficTrackerCallback();

        int port = getFreePort();
        ix::WebSocketServer server(port);
        std::atomic<int> serverReceivedPingMessages(0);
        REQUIRE(startServer(server, serverReceivedPingMessages));

        std::string session = ix::generateSessionId();
        bool useSetHeartBeatPeriodMethod = true;
        WebSocketClient webSocketClient(port, useSetHeartBeatPeriodMethod);

        webSocketClient.start();

        // Wait for all chat instance to be ready
        while (true)
        {
            if (webSocketClient.isReady()) break;
            ix::msleep(10);
        }

        REQUIRE(server.getClients().size() == 1);

        ix::msleep(900);
        webSocketClient.sendMessage("hello world");
        ix::msleep(900);
        webSocketClient.sendMessage("hello world");
        ix::msleep(1100);

        webSocketClient.stop();

        // Here we test ping interval
        // client has sent data, but ping should have been sent no matter what
        // -> expected ping messages == 2 as 900+900+1100 = 2900 seconds, 1 ping sent every second
        REQUIRE(serverReceivedPingMessages == 2);

        // Give us 500ms for the server to notice that clients went away
        ix::msleep(500);
        REQUIRE(server.getClients().size() == 0);

        ix::reportWebSocketTraffic();
    }
}

TEST_CASE("Websocket_ping_no_data_sent_setPingInterval", "[setPingInterval]")
{
    SECTION("Make sure that ping messages are sent when no other data are sent.")
    {
        ix::setupWebSocketTrafficTrackerCallback();

        int port = getFreePort();
        ix::WebSocketServer server(port);
        std::atomic<int> serverReceivedPingMessages(0);
        REQUIRE(startServer(server, serverReceivedPingMessages));

        std::string session = ix::generateSessionId();
        bool useSetHeartBeatPeriodMethod = false; // so use setPingInterval
        WebSocketClient webSocketClient(port, useSetHeartBeatPeriodMethod);

        webSocketClient.start();

        // Wait for all chat instance to be ready
        while (true)
        {
            if (webSocketClient.isReady()) break;
            ix::msleep(10);
        }

        REQUIRE(server.getClients().size() == 1);

        ix::msleep(2100);

        webSocketClient.stop();


        // Here we test ping interval
        // -> expected ping messages == 2 as 2100 seconds, 1 ping sent every second
        REQUIRE(serverReceivedPingMessages == 2);

        // Give us 500ms for the server to notice that clients went away
        ix::msleep(500);
        REQUIRE(server.getClients().size() == 0);

        ix::reportWebSocketTraffic();
    }
}

TEST_CASE("Websocket_ping_data_sent_setPingInterval", "[setPingInterval]")
{
    SECTION("Make sure that ping messages are sent, even if other messages are sent")
    {
        ix::setupWebSocketTrafficTrackerCallback();

        int port = getFreePort();
        ix::WebSocketServer server(port);
        std::atomic<int> serverReceivedPingMessages(0);
        REQUIRE(startServer(server, serverReceivedPingMessages));

        std::string session = ix::generateSessionId();
        bool useSetHeartBeatPeriodMethod = false; // so use setPingInterval
        WebSocketClient webSocketClient(port, useSetHeartBeatPeriodMethod);

        webSocketClient.start();

        // Wait for all chat instance to be ready
        while (true)
        {
            if (webSocketClient.isReady()) break;
            ix::msleep(10);
        }

        REQUIRE(server.getClients().size() == 1);

        ix::msleep(900);
        webSocketClient.sendMessage("hello world");
        ix::msleep(900);
        webSocketClient.sendMessage("hello world");
        ix::msleep(1300);

        webSocketClient.stop();

        // Here we test ping interval
        // client has sent data, but ping should have been sent no matter what
        // -> expected ping messages == 3 as 900+900+1300 = 3100 seconds, 1 ping sent every second
        REQUIRE(serverReceivedPingMessages == 3);

        // Give us 500ms for the server to notice that clients went away
        ix::msleep(500);
        REQUIRE(server.getClients().size() == 0);

        ix::reportWebSocketTraffic();
    }
}
