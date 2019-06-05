/*
 *  IXWebSocketCloseTest.cpp
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
            void stop(uint16_t code, const std::string& reason);
            bool isReady() const;
            void sendMessage(const std::string& text);

            uint16_t getCloseCode();
            const std::string& getCloseReason();
            bool getCloseRemote();

        private:
            ix::WebSocket _webSocket;
            int _port;

            mutable std::mutex _mutexCloseData;
            uint16_t _closeCode;
            std::string _closeReason;
            bool _closeRemote;
    };

    WebSocketClient::WebSocketClient(int port)
        : _port(port)
        , _closeCode(0)
        , _closeReason(std::string(""))
        , _closeRemote(false)
    {
        ;
    }

    bool WebSocketClient::isReady() const
    {
        return _webSocket.getReadyState() == ix::ReadyState::Open;
    }

    uint16_t WebSocketClient::getCloseCode()
    {
        std::lock_guard<std::mutex> lck(_mutexCloseData);

        return _closeCode;
    }

    const std::string& WebSocketClient::getCloseReason()
    {
        std::lock_guard<std::mutex> lck(_mutexCloseData);

        return _closeReason;
    }

    bool WebSocketClient::getCloseRemote()
    {
        std::lock_guard<std::mutex> lck(_mutexCloseData);

        return _closeRemote;
    }

    void WebSocketClient::stop()
    {
        _webSocket.stop();
    }

    void WebSocketClient::stop(uint16_t code, const std::string& reason)
    {
        _webSocket.stop(code, reason);
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
        _webSocket.disableAutomaticReconnection();

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
                if (messageType == ix::WebSocketMessageType::Open)
                {
                    log("client connected");
                }
                else if (messageType == ix::WebSocketMessageType::Close)
                {
                    std::stringstream ss;
                    ss << "client disconnected("
                       << closeInfo.code
                       << ","
                       << closeInfo.reason
                       << ")";
                    log(ss.str());

                    std::lock_guard<std::mutex> lck(_mutexCloseData);

                    _closeCode = closeInfo.code;
                    _closeReason = std::string(closeInfo.reason);
                    _closeRemote = closeInfo.remote;
                }
                else if (messageType == ix::WebSocketMessageType::Error)
                {
                    ss << "Error ! " << error.reason;
                    log(ss.str());
                }
                else if (messageType == ix::WebSocketMessageType::Pong)
                {
                    ss << "Received pong message " << str;
                    log(ss.str());
                }
                else if (messageType == ix::WebSocketMessageType::Ping)
                {
                    ss << "Received ping message " << str;
                    log(ss.str());
                }
                else if (messageType == ix::WebSocketMessageType::Message)
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

    bool startServer(ix::WebSocketServer& server,
                     uint16_t& receivedCloseCode,
                     std::string& receivedCloseReason,
                     bool& receivedCloseRemote,
                     std::mutex& mutexWrite)
    {
        // A dev/null server
        server.setOnConnectionCallback(
            [&server, &receivedCloseCode, &receivedCloseReason, &receivedCloseRemote, &mutexWrite](std::shared_ptr<ix::WebSocket> webSocket,
                                             std::shared_ptr<ConnectionState> connectionState)
            {
                webSocket->setOnMessageCallback(
                    [webSocket, connectionState, &server, &receivedCloseCode, &receivedCloseReason, &receivedCloseRemote, &mutexWrite](ix::WebSocketMessageType messageType,
                       const std::string& str,
                       size_t wireSize,
                       const ix::WebSocketErrorInfo& error,
                       const ix::WebSocketOpenInfo& openInfo,
                       const ix::WebSocketCloseInfo& closeInfo)
                    {
                        if (messageType == ix::WebSocketMessageType::Open)
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
                        else if (messageType == ix::WebSocketMessageType::Close)
                        {
                            std::stringstream ss;
                            ss << "Server closed connection("
                               << closeInfo.code
                               << ","
                               << closeInfo.reason
                               << ")";
                            log(ss.str());

                            std::lock_guard<std::mutex> lck(mutexWrite);

                            receivedCloseCode = closeInfo.code;
                            receivedCloseReason = std::string(closeInfo.reason);
                            receivedCloseRemote = closeInfo.remote;
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

TEST_CASE("Websocket_client_close_default", "[close]")
{
    SECTION("Make sure that close code and reason was used and sent to server.")
    {
        ix::setupWebSocketTrafficTrackerCallback();

        int port = getFreePort();
        ix::WebSocketServer server(port);

        uint16_t serverReceivedCloseCode(0);
        bool serverReceivedCloseRemote(false);
        std::string serverReceivedCloseReason("");
        std::mutex mutexWrite;

        REQUIRE(startServer(server, serverReceivedCloseCode, serverReceivedCloseReason, serverReceivedCloseRemote, mutexWrite));

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

        ix::msleep(500);

        webSocketClient.stop();

        ix::msleep(500);

        // ensure client close is the same as values given
        REQUIRE(webSocketClient.getCloseCode() == 1000);
        REQUIRE(webSocketClient.getCloseReason() == "Normal closure");
        REQUIRE(webSocketClient.getCloseRemote() == false);

        {
            std::lock_guard<std::mutex> lck(mutexWrite);

            // Here we read the code/reason received by the server, and ensure that remote is true
            REQUIRE(serverReceivedCloseCode == 1000);
            REQUIRE(serverReceivedCloseReason == "Normal closure");
            REQUIRE(serverReceivedCloseRemote == true);
        }

        // Give us 1000ms for the server to notice that clients went away
        ix::msleep(1000);
        REQUIRE(server.getClients().size() == 0);

        ix::reportWebSocketTraffic();
    }
}

TEST_CASE("Websocket_client_close_params_given", "[close]")
{
    SECTION("Make sure that close code and reason was used and sent to server.")
    {
        ix::setupWebSocketTrafficTrackerCallback();

        int port = getFreePort();
        ix::WebSocketServer server(port);

        uint16_t serverReceivedCloseCode(0);
        bool serverReceivedCloseRemote(false);
        std::string serverReceivedCloseReason("");
        std::mutex mutexWrite;

        REQUIRE(startServer(server, serverReceivedCloseCode, serverReceivedCloseReason, serverReceivedCloseRemote, mutexWrite));

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

        ix::msleep(500);

        webSocketClient.stop(4000, "My reason");

        ix::msleep(500);

        // ensure client close is the same as values given
        REQUIRE(webSocketClient.getCloseCode() == 4000);
        REQUIRE(webSocketClient.getCloseReason() == "My reason");
        REQUIRE(webSocketClient.getCloseRemote() == false);

        {
            std::lock_guard<std::mutex> lck(mutexWrite);

            // Here we read the code/reason received by the server, and ensure that remote is true
            REQUIRE(serverReceivedCloseCode == 4000);
            REQUIRE(serverReceivedCloseReason == "My reason");
            REQUIRE(serverReceivedCloseRemote == true);
        }

        // Give us 1000ms for the server to notice that clients went away
        ix::msleep(1000);
        REQUIRE(server.getClients().size() == 0);

        ix::reportWebSocketTraffic();
    }
}

TEST_CASE("Websocket_server_close", "[close]")
{
    SECTION("Make sure that close code and reason was read from server.")
    {
        ix::setupWebSocketTrafficTrackerCallback();

        int port = getFreePort();
        ix::WebSocketServer server(port);

        uint16_t serverReceivedCloseCode(0);
        bool serverReceivedCloseRemote(false);
        std::string serverReceivedCloseReason("");
        std::mutex mutexWrite;

        REQUIRE(startServer(server, serverReceivedCloseCode, serverReceivedCloseReason, serverReceivedCloseRemote, mutexWrite));

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

        ix::msleep(500);

        server.stop();

        ix::msleep(500);

        // ensure client close is the same as values given
        REQUIRE(webSocketClient.getCloseCode() == 1000);
        REQUIRE(webSocketClient.getCloseReason() == "Normal closure");
        REQUIRE(webSocketClient.getCloseRemote() == true);

        {
            std::lock_guard<std::mutex> lck(mutexWrite);

            // Here we read the code/reason received by the server, and ensure that remote is true
            REQUIRE(serverReceivedCloseCode == 1000);
            REQUIRE(serverReceivedCloseReason == "Normal closure");
            REQUIRE(serverReceivedCloseRemote == false);
        }

        // Give us 1000ms for the server to notice that clients went away
        ix::msleep(1000);
        REQUIRE(server.getClients().size() == 0);

        ix::reportWebSocketTraffic();
    }
}

TEST_CASE("Websocket_server_close_immediatly", "[close]")
{
    SECTION("Make sure that close code and reason was read from server.")
    {
        ix::setupWebSocketTrafficTrackerCallback();

        int port = getFreePort();
        ix::WebSocketServer server(port);

        uint16_t serverReceivedCloseCode(0);
        bool serverReceivedCloseRemote(false);
        std::string serverReceivedCloseReason("");
        std::mutex mutexWrite;

        REQUIRE(startServer(server, serverReceivedCloseCode, serverReceivedCloseReason, serverReceivedCloseRemote, mutexWrite));

        std::string session = ix::generateSessionId();
        WebSocketClient webSocketClient(port);

        webSocketClient.start();

        server.stop();

        ix::msleep(500);

        // ensure client close hasn't been called
        REQUIRE(webSocketClient.getCloseCode() == 0);
        REQUIRE(webSocketClient.getCloseReason() == "");
        REQUIRE(webSocketClient.getCloseRemote() == false);

        {
            std::lock_guard<std::mutex> lck(mutexWrite);

            // Here we ensure that the code/reason wasn't received by the server
            REQUIRE(serverReceivedCloseCode == 0);
            REQUIRE(serverReceivedCloseReason == "");
            REQUIRE(serverReceivedCloseRemote == false);
        }

        // Give us 1000ms for the server to notice that clients went away
        ix::msleep(1000);
        REQUIRE(server.getClients().size() == 0);

        ix::reportWebSocketTraffic();
    }
}
