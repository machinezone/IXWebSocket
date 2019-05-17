/*
 *  IXWebSocketMessageQTest.cpp
 *  Author: Korchynskyi Dmytro
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXWebSocketMessageQueue.h>

#include "IXTest.h"
#include "catch.hpp"
#include <thread>

using namespace ix;

namespace
{
    bool startServer(ix::WebSocketServer& server)
    {
        server.setOnConnectionCallback(
            [&server](std::shared_ptr<ix::WebSocket> webSocket,
                std::shared_ptr<ConnectionState> connectionState)
        {
            webSocket->setOnMessageCallback(
                [connectionState, &server](ix::WebSocketMessageType messageType,
                    const std::string & str,
                    size_t wireSize,
                    const ix::WebSocketErrorInfo & error,
                    const ix::WebSocketOpenInfo & openInfo,
                    const ix::WebSocketCloseInfo & closeInfo)
            {
                if (messageType == ix::WebSocketMessageType::Open)
                {
                    Logger() << "New connection";
                    connectionState->computeId();
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
                    Logger() << "Closed connection";
                }
                else if (messageType == ix::WebSocketMessageType::Message)
                {
                    Logger() << "Message received: " << str;

                    for (auto&& client : server.getClients())
                    {
                        client->send(str);
                    }
                }
            }
            );
        }
        );

        auto res = server.listen();
        if (!res.first)
        {
            Logger() << res.second;
            return false;
        }

        server.start();
        return true;
    }

    class MsgQTestClient
    {
    public:
        MsgQTestClient()
        {
            msgQ.bindWebsocket(&ws);

            msgQ.setOnMessageCallback([this](WebSocketMessageType messageType,
                const std::string & str,
                size_t wireSize,
                const WebSocketErrorInfo & error,
                const WebSocketOpenInfo & openInfo,
                const WebSocketCloseInfo & closeInfo)
            {
                REQUIRE(mainThreadId == std::this_thread::get_id());

                std::stringstream ss;
                if (messageType == WebSocketMessageType::Open)
                {
                    log("client connected");
                    sendNextMessage();
                }
                else if (messageType == WebSocketMessageType::Close)
                {
                    log("client disconnected");
                }
                else if (messageType == WebSocketMessageType::Error)
                {
                    ss << "Error ! " << error.reason;
                    log(ss.str());
                    testDone = true;
                }
                else if (messageType == WebSocketMessageType::Pong)
                {
                    ss << "Received pong message " << str;
                    log(ss.str());
                }
                else if (messageType == WebSocketMessageType::Ping)
                {
                    ss << "Received ping message " << str;
                    log(ss.str());
                }
                else if (messageType == WebSocketMessageType::Message)
                {
                    REQUIRE(str.compare("Hey dude!") == 0);
                    ++receivedCount;
                    ss << "Received message " << str;
                    log(ss.str());
                    sendNextMessage();
                }
                else
                {
                    ss << "Invalid WebSocketMessageType";
                    log(ss.str());
                    testDone = true;
                }
            });
        }

        void sendNextMessage()
        {
            if (receivedCount >= 3)
            {
                testDone = true;
                succeeded = true;
            }
            else
            {
                auto info = ws.sendText("Hey dude!");
                if (info.success)
                    log("sent message");
                else
                    log("send failed");
            }
        }

        void run(const std::string& url)
        {
            mainThreadId = std::this_thread::get_id();
            testDone = false;
            receivedCount = 0;

            ws.setUrl(url);
            ws.start();

            while (!testDone)
            {
                msgQ.poll();
                msleep(50);
            }
        }

        bool isSucceeded() const { return succeeded; }

    private:
        WebSocket ws;
        WebSocketMessageQueue msgQ;
        bool testDone = false;
        uint32_t receivedCount = 0;
        std::thread::id mainThreadId;
        bool succeeded = false;
    };
}

TEST_CASE("Websocket_message_queue", "[websocket_message_q]")
{
    SECTION("Send several messages")
    {
        int port = getFreePort();
        WebSocketServer server(port);
        REQUIRE(startServer(server));

        MsgQTestClient testClient;
        testClient.run("ws://127.0.0.1:" + std::to_string(port));
        REQUIRE(testClient.isSucceeded());
    }

}
