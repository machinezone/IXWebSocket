/*
 *  ws_chat.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2019 Machine Zone, Inc. All rights reserved.
 */

//
// Simple chat program that talks to a broadcast server
// Broadcast server can be ran with `ws broadcast_server`
//

#include <iostream>
#include <sstream>
#include <queue>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXSocket.h>

#include "nlohmann/json.hpp"

// for convenience
using json = nlohmann::json;

namespace ix
{
    class WebSocketChat
    {
        public:
            WebSocketChat(const std::string& url,
                          const std::string& user);

            void subscribe(const std::string& channel);
            void start();
            void stop();
            bool isReady() const;

            void sendMessage(const std::string& text);
            size_t getReceivedMessagesCount() const;

            std::string encodeMessage(const std::string& text);
            std::pair<std::string, std::string> decodeMessage(const std::string& str);

        private:
            std::string _url;
            std::string _user;
            ix::WebSocket _webSocket;
            std::queue<std::string> _receivedQueue;

            void log(const std::string& msg);
    };

    WebSocketChat::WebSocketChat(const std::string& url,
                                 const std::string& user) :
        _url(url),
        _user(user)
    {
        ;
    }

    void WebSocketChat::log(const std::string& msg)
    {
        std::cout << msg << std::endl;
    }

    size_t WebSocketChat::getReceivedMessagesCount() const
    {
        return _receivedQueue.size();
    }

    bool WebSocketChat::isReady() const
    {
        return _webSocket.getReadyState() == ix::ReadyState::Open;
    }

    void WebSocketChat::stop()
    {
        _webSocket.stop();
    }

    void WebSocketChat::start()
    {
        _webSocket.setUrl(_url);

        std::stringstream ss;
        log(std::string("Connecting to url: ") + _url);

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
                    log("ws chat: connected");
                    std::cout << "Uri: " << openInfo.uri << std::endl;
                    std::cout << "Handshake Headers:" << std::endl;
                    for (auto it : openInfo.headers)
                    {
                        std::cout << it.first << ": " << it.second << std::endl;
                    }

                    ss << "ws chat: user "
                       << _user
                       << " Connected !";
                       log(ss.str());
                }
                else if (messageType == ix::WebSocketMessageType::Close)
                {
                    ss << "ws chat: user "
                       << _user
                       << " disconnected !"
                       << " code " << closeInfo.code
                       << " reason " << closeInfo.reason;
                       log(ss.str());
                }
                else if (messageType == ix::WebSocketMessageType::Message)
                {
                    auto result = decodeMessage(str);

                    // Our "chat" / "broacast" node.js server does not send us
                    // the messages we send, so we don't have to filter it out.

                    // store text
                    _receivedQueue.push(result.second);

                    ss << std::endl
                       << result.first << "(" << wireSize << " bytes)" << " > " << result.second
                       << std::endl
                       << _user << " > ";
                    log(ss.str());
                }
                else if (messageType == ix::WebSocketMessageType::Error)
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

    int ws_chat_main(const std::string& url,
                     const std::string& user)
    {
        std::cout << "Type Ctrl-D to exit prompt..." << std::endl;
        WebSocketChat webSocketChat(url, user);
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

        return 0;
    }
}
