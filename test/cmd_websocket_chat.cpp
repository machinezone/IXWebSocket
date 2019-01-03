/*
 *  cmd_websocket_chat.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017 Machine Zone. All rights reserved.
 */

//
// Simple chat program that talks to the node.js server at
// websocket_chat_server/broacast-server.js
//

#include <iostream>
#include <sstream>
#include <queue>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include "msgpack11.hpp"

#include "IXTest.h"

#include "catch.hpp"

using msgpack11::MsgPack;
using namespace ix;

namespace
{
    class WebSocketChat
    {
        public:
            WebSocketChat(const std::string& user,
                       const std::string& session);

            void subscribe(const std::string& channel);
            void start();
            void stop();
            bool isReady() const;

            void sendMessage(const std::string& text);
            size_t getReceivedMessagesCount() const;

            std::string encodeMessage(const std::string& text);
            std::pair<std::string, std::string> decodeMessage(const std::string& str);

        private:
            void log(const std::string& msg);

            std::string _user;
            std::string _session;

            ix::WebSocket _webSocket;

            std::queue<std::string> _receivedQueue;
    };

    WebSocketChat::WebSocketChat(const std::string& user,
                                 const std::string& session) :
        _user(user),
        _session(session)
    {
        ;
    }

    size_t WebSocketChat::getReceivedMessagesCount() const
    {
        return _receivedQueue.size();
    }

    bool WebSocketChat::isReady() const
    {
        return _webSocket.getReadyState() == ix::WebSocket_ReadyState_Open;
    }

    void WebSocketChat::stop()
    {
        _webSocket.stop();
    }

    void WebSocketChat::log(const std::string& msg)
    {
        std::cerr << msg << std::endl;
    }

    void WebSocketChat::start()
    {
        std::string url("ws://localhost:8090/");
        _webSocket.setUrl(url);

        std::stringstream ss;
        log(std::string("Connecting to url: ") + url);

        _webSocket.setOnMessageCallback(
            [this](ix::WebSocketMessageType messageType,
                   const std::string& str,
                   size_t wireSize,
                   const ix::WebSocketErrorInfo& error,
                   const ix::WebSocketCloseInfo& closeInfo,
                   const ix::WebSocketHttpHeaders& headers)
            {
                std::stringstream ss;
                if (messageType == ix::WebSocket_MessageType_Open)
                {
                    ss << "cmd_websocket_chat: user "
                       << _user
                       << " Connected !";
                    log(ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Close)
                {
                    ss << "cmd_websocket_chat: user "
                       << _user
                       << " disconnected !";
                    log(ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Message)
                {
                    auto result = decodeMessage(str);

                    // Our "chat" / "broacast" node.js server does not send us
                    // the messages we send, so we don't need to have a msg_user != user
                    // as we do for the satori chat example.

                    // store text
                    _receivedQueue.push(result.second);

                    ss << std::endl 
                       << result.first << " > " << result.second
                       << std::endl
                       << _user << " > ";
                    log(ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Error)
                {
                    ss << "cmd_websocket_chat: Error ! " << error.reason;
                    log(ss.str());
                }
                else
                {
                    // FIXME: missing ping/pong messages
                    ss << "Invalid ix::WebSocketMessageType";
                    log(ss.str());
                }
            });

        _webSocket.start();
    }

    std::pair<std::string, std::string> WebSocketChat::decodeMessage(const std::string& str)
    {
        std::string errMsg;
        MsgPack msg = MsgPack::parse(str, errMsg);

        std::string msg_user = msg["user"].string_value();
        std::string msg_text = msg["text"].string_value();

        return std::pair<std::string, std::string>(msg_user, msg_text);
    }

    std::string WebSocketChat::encodeMessage(const std::string& text)
    {
        std::map<MsgPack, MsgPack> obj;
        obj["user"] = _user;
        obj["text"] = text;

        MsgPack msg(obj);

        std::string output = msg.dump();
        return output;
    }

    void WebSocketChat::sendMessage(const std::string& text)
    {
        _webSocket.send(encodeMessage(text));
    }

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

TEST_CASE("Websocket_chat", "[websocket_chat]")
{
    SECTION("Exchange and count sent/received messages.")
    {
        ix::setupWebSocketTrafficTrackerCallback();

        int port = 8090;
        ix::WebSocketServer server(port);
        REQUIRE(startServer(server));

        std::string session = ix::generateSessionId();
        WebSocketChat chatA("jean", session);
        WebSocketChat chatB("paul", session);

        chatA.start();
        chatB.start();

        // Wait for all chat instance to be ready
        while (true)
        {
            if (chatA.isReady() && chatB.isReady()) break;
            ix::msleep(10);
        }

        REQUIRE(server.getClients().size() == 2);

        // Add a bit of extra time, for the subscription to be active
        ix::msleep(200);

        chatA.sendMessage("from A1");
        chatA.sendMessage("from A2");
        chatA.sendMessage("from A3");

        chatB.sendMessage("from B1");
        chatB.sendMessage("from B2");

        // Give us 1s for all messages to be received
        ix::msleep(1000);

        chatA.stop();
        chatB.stop();

        REQUIRE(chatA.getReceivedMessagesCount() == 2);
        REQUIRE(chatB.getReceivedMessagesCount() == 3);

        // Give us 500ms for the server to notice that clients went away
        ix::msleep(500);
        REQUIRE(server.getClients().size() == 0);

        ix::reportWebSocketTraffic();
    }
}
