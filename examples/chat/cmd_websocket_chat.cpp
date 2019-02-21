/*
 *  cmd_websocket_chat.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

//
// Simple chat program that talks to the node.js server at
// websocket_chat_server/broacast-server.js
//

#include <iostream>
#include <sstream>
#include <queue>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXSocket.h>

#include "nlohmann/json.hpp"

// for convenience
using json = nlohmann::json;

using namespace ix;

namespace
{
    void log(const std::string& msg)
    {
        std::cout << msg << std::endl;
    }

    class WebSocketChat
    {
        public:
            WebSocketChat(const std::string& user);

            void subscribe(const std::string& channel);
            void start();
            void stop();
            bool isReady() const;

            void sendMessage(const std::string& text);
            size_t getReceivedMessagesCount() const;

            std::string encodeMessage(const std::string& text);
            std::pair<std::string, std::string> decodeMessage(const std::string& str);

        private:
            std::string _user;

            ix::WebSocket _webSocket;

            std::queue<std::string> _receivedQueue;
    };

    WebSocketChat::WebSocketChat(const std::string& user) :
        _user(user)
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

    void WebSocketChat::start()
    {
        std::string url("ws://localhost:8080/");
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
                    // the messages we send, so we don't have to filter it out.

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
                    ss << "Connection error: " << error.reason      << std::endl;
                    ss << "#retries: "         << error.retries     << std::endl;
                    ss << "Wait time(ms): "    << error.wait_time   << std::endl;
                    ss << "HTTP Status: "      << error.http_status << std::endl;
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

    std::pair<std::string, std::string> WebSocketChat::decodeMessage(const std::string& str)
    {
        auto j = json::parse(str);

        std::string msg_user = j["user"];
        std::string msg_text = j["text"];

        return std::pair<std::string, std::string>(msg_user, msg_text);
    }

    std::string WebSocketChat::encodeMessage(const std::string& text)
    {
        json j;
        j["user"] = _user;
        j["text"] = text;

        std::string output = j.dump();
        return output;
    }

    void WebSocketChat::sendMessage(const std::string& text)
    {
        _webSocket.send(encodeMessage(text));
    }

    void interactiveMain(const std::string& user)
    {
        std::cout << "Type Ctrl-D to exit prompt..." << std::endl;
        WebSocketChat webSocketChat(user);
        webSocketChat.start();

        while (true)
        {
            std::string text;
            std::cout << user << " > " << std::flush;
            std::getline(std::cin, text);

            if (!std::cin)
            {
                break;
            }

            webSocketChat.sendMessage(text);
        }

        std::cout << std::endl;
        webSocketChat.stop();
    }
}

int main(int argc, char** argv)
{
    std::string user("user");
    if (argc == 2)
    {
        user = argv[1];
    }

    Socket::init();
    interactiveMain(user);
    return 0;
}
