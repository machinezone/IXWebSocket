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
#include <vector>
#include <mutex>
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
                          const std::string& session,
                          int port);

            void subscribe(const std::string& channel);
            void start();
            void stop();
            bool isReady() const;

            void sendMessage(const std::string& text);
            size_t getReceivedMessagesCount() const;
            const std::vector<std::string>& getReceivedMessages() const;

            std::string encodeMessage(const std::string& text);
            std::pair<std::string, std::string> decodeMessage(const std::string& str);
            void appendMessage(const std::string& message);

        private:
            std::string _user;
            std::string _session;
            int _port;

            ix::WebSocket _webSocket;

            std::vector<std::string> _receivedMessages;
            mutable std::mutex _mutex;
    };

    WebSocketChat::WebSocketChat(const std::string& user,
                                 const std::string& session,
                                 int port) :
        _user(user),
        _session(session),
        _port(port)
    {
        ;
    }

    size_t WebSocketChat::getReceivedMessagesCount() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _receivedMessages.size();
    }

    const std::vector<std::string>& WebSocketChat::getReceivedMessages() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _receivedMessages;
    }

    void WebSocketChat::appendMessage(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _receivedMessages.push_back(message);
    }

    bool WebSocketChat::isReady() const
    {
        return _webSocket.getReadyState() == ix::WebSocket_ReadyState_Open;
    }

    void WebSocketChat::stop()
    {
        _webSocket.stop();
    }

    void WebSocketChat::start()
    {
        std::string url;
        {
            std::stringstream ss;
            ss << "ws://127.0.0.1:"
               << _port
               << "/"
               << _user;

            url = ss.str();
        }

        _webSocket.setUrl(url);

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
                    appendMessage(result.second);

                    std::string payload = result.second;
                    if (payload.size() > 2000)
                    {
                        payload = "<message too large>";
                    }

                    ss << std::endl
                       << result.first << " > " << payload
                       << std::endl
                       << _user << " > ";
                    log(ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Error)
                {
                    ss << "cmd_websocket_chat: Error ! " << error.reason;
                    log(ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Ping)
                {
                    log("cmd_websocket_chat: received ping message");
                }
                else if (messageType == ix::WebSocket_MessageType_Pong)
                {
                    log("cmd_websocket_chat: received pong message");
                }
                else if (messageType == ix::WebSocket_MessageType_Fragment)
                {
                    log("cmd_websocket_chat: received message fragment");
                }
                else
                {
                    ss << "Unexpected ix::WebSocketMessageType";
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
            [&server](std::shared_ptr<ix::WebSocket> webSocket,
                      std::shared_ptr<ConnectionState> connectionState)
            {
                webSocket->setOnMessageCallback(
                    [webSocket, connectionState, &server](ix::WebSocketMessageType messageType,
                       const std::string& str,
                       size_t wireSize,
                       const ix::WebSocketErrorInfo& error,
                       const ix::WebSocketOpenInfo& openInfo,
                       const ix::WebSocketCloseInfo& closeInfo)
                    {
                        if (messageType == ix::WebSocket_MessageType_Open)
                        {
                            Logger() << "New connection";
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
                            log("Closed connection");
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

TEST_CASE("Websocket_chat", "[websocket_chat]")
{
    SECTION("Exchange and count sent/received messages.")
    {
        ix::setupWebSocketTrafficTrackerCallback();

        int port = 8090;
        ix::WebSocketServer server(port);
        REQUIRE(startServer(server));

        std::string session = ix::generateSessionId();
        WebSocketChat chatA("jean", session, port);
        WebSocketChat chatB("paul", session, port);

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

        // Test large messages that needs to be broken into small fragments
        size_t size = 1 * 1024 * 1024; // ~1Mb
        std::string bigMessage(size, 'a');
        chatB.sendMessage(bigMessage);

        log("Sent all messages");

        // Wait until all messages are received. 10s timeout
        int attempts = 0;
        while (chatA.getReceivedMessagesCount() != 3 ||
               chatB.getReceivedMessagesCount() != 3)
        {
            REQUIRE(attempts++ < 10);
            ix::msleep(1000);
        }

        chatA.stop();
        chatB.stop();

        REQUIRE(chatA.getReceivedMessagesCount() == 3);
        REQUIRE(chatB.getReceivedMessagesCount() == 3);

        REQUIRE(chatB.getReceivedMessages()[0] == "from A1");
        REQUIRE(chatB.getReceivedMessages()[1] == "from A2");
        REQUIRE(chatB.getReceivedMessages()[2] == "from A3");

        REQUIRE(chatA.getReceivedMessages()[0] == "from B1");
        REQUIRE(chatA.getReceivedMessages()[1] == "from B2");
        REQUIRE(chatA.getReceivedMessages()[2].size() == bigMessage.size());

        // Give us 500ms for the server to notice that clients went away
        ix::msleep(500);
        REQUIRE(server.getClients().size() == 0);

        ix::reportWebSocketTraffic();
    }
}
