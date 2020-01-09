/*
 *  ws_chat.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2019 Machine Zone, Inc. All rights reserved.
 */

//
// Simple chat program that talks to a broadcast server
// Broadcast server can be ran with `ws broadcast_server`
//

#include "nlohmann/json.hpp"
#include <iostream>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXWebSocket.h>
#include <queue>
#include <spdlog/spdlog.h>
#include <sstream>

// for convenience
using json = nlohmann::json;

namespace ix
{
    class WebSocketChat
    {
    public:
        WebSocketChat(const std::string& url, const std::string& user);

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

    WebSocketChat::WebSocketChat(const std::string& url, const std::string& user)
        : _url(url)
        , _user(user)
    {
        ;
    }

    void WebSocketChat::log(const std::string& msg)
    {
        spdlog::info(msg);
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

        _webSocket.setOnMessageCallback([this](const WebSocketMessagePtr& msg) {
            std::stringstream ss;
            if (msg->type == ix::WebSocketMessageType::Open)
            {
                log("ws chat: connected");
                spdlog::info("Uri: {}", msg->openInfo.uri);
                spdlog::info("Headers:");
                for (auto it : msg->openInfo.headers)
                {
                    spdlog::info("{}: {}", it.first, it.second);
                }

                spdlog::info("ws chat: user {} connected !", _user);
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                ss << "ws chat user disconnected: " << _user;
                ss << " code " << msg->closeInfo.code;
                ss << " reason " << msg->closeInfo.reason << std::endl;
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Message)
            {
                auto result = decodeMessage(msg->str);

                // Our "chat" / "broacast" node.js server does not send us
                // the messages we send, so we don't have to filter it out.

                // store text
                _receivedQueue.push(result.second);

                ss << std::endl
                   << result.first << "(" << msg->wireSize << " bytes)"
                   << " > " << result.second << std::endl
                   << _user << " > ";
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                ss << "Connection error: " << msg->errorInfo.reason << std::endl;
                ss << "#retries: " << msg->errorInfo.retries << std::endl;
                ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
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
        _webSocket.sendText(encodeMessage(text));
    }

    int ws_chat_main(const std::string& url, const std::string& user)
    {
        spdlog::info("Type Ctrl-D to exit prompt...");
        WebSocketChat webSocketChat(url, user);
        webSocketChat.start();

        while (true)
        {
            // Read line
            std::string line;
            std::cout << user << " > " << std::flush;
            std::getline(std::cin, line);

            if (!std::cin)
            {
                break;
            }

            webSocketChat.sendMessage(line);
        }

        spdlog::info("");
        webSocketChat.stop();

        return 0;
    }
} // namespace ix
