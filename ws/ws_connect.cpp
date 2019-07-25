/*
 *  ws_connect.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXSocket.h>

#include "linenoise.hpp"


namespace ix
{
    class WebSocketConnect
    {
        public:
            WebSocketConnect(const std::string& _url,
                             bool disableAutomaticReconnection,
                             bool disablePerMessageDeflate);

            void subscribe(const std::string& channel);
            void start();
            void stop();

            void sendMessage(const std::string& text);

        private:
            std::string _url;
            ix::WebSocket _webSocket;
            bool _disablePerMessageDeflate;

            void log(const std::string& msg);
    };

    WebSocketConnect::WebSocketConnect(const std::string& url,
                                       bool disableAutomaticReconnection,
                                       bool disablePerMessageDeflate) :
        _url(url),
        _disablePerMessageDeflate(disablePerMessageDeflate)
    {
        if (disableAutomaticReconnection)
        {
            _webSocket.disableAutomaticReconnection();
        }
    }

    void WebSocketConnect::log(const std::string& msg)
    {
        std::cout << msg << std::endl;
    }

    void WebSocketConnect::stop()
    {
        _webSocket.stop();
    }

    void WebSocketConnect::start()
    {
        _webSocket.setUrl(_url);

        if (_disablePerMessageDeflate)
        {
            _webSocket.disablePerMessageDeflate();
        }
        else
        {
            ix::WebSocketPerMessageDeflateOptions webSocketPerMessageDeflateOptions(
                true, false, false, 15, 15);
            _webSocket.setPerMessageDeflateOptions(webSocketPerMessageDeflateOptions);
        }

        std::stringstream ss;
        log(std::string("Connecting to url: ") + _url);

        _webSocket.setOnMessageCallback(
            [this](const ix::WebSocketMessagePtr& msg)
            {
                std::stringstream ss;
                if (msg->type == ix::WebSocketMessageType::Open)
                {
                    log("ws_connect: connected");
                    std::cout << "Uri: " << msg->openInfo.uri << std::endl;
                    std::cout << "Handshake Headers:" << std::endl;
                    for (auto it : msg->openInfo.headers)
                    {
                        std::cout << it.first << ": " << it.second << std::endl;
                    }
                }
                else if (msg->type == ix::WebSocketMessageType::Close)
                {
                    ss << "ws_connect: connection closed:";
                    ss << " code " << msg->closeInfo.code;
                    ss << " reason " << msg->closeInfo.reason << std::endl;
                    log(ss.str());
                }
                else if (msg->type == ix::WebSocketMessageType::Message)
                {
                    std::cerr << "Received " << msg->wireSize << " bytes" << std::endl;

                    ss << "ws_connect: received message: "
                       << msg->str;
                    log(ss.str());
                }
                else if (msg->type == ix::WebSocketMessageType::Error)
                {
                    ss << "Connection error: " << msg->errorInfo.reason      << std::endl;
                    ss << "#retries: "         << msg->errorInfo.retries     << std::endl;
                    ss << "Wait time(ms): "    << msg->errorInfo.wait_time   << std::endl;
                    ss << "HTTP Status: "      << msg->errorInfo.http_status << std::endl;
                    log(ss.str());
                }
                else if (msg->type == ix::WebSocketMessageType::Fragment)
                {
                    std::cerr << "Received message fragment" << std::endl;
                }
                else if (msg->type == ix::WebSocketMessageType::Ping)
                {
                    std::cerr << "Received ping" << std::endl;
                }
                else if (msg->type == ix::WebSocketMessageType::Pong)
                {
                    std::cerr << "Received pong" << std::endl;
                }
                else
                {
                    ss << "Invalid ix::WebSocketMessageType";
                    log(ss.str());
                }
            });

        _webSocket.start();
    }

    void WebSocketConnect::sendMessage(const std::string& text)
    {
        _webSocket.sendText(text);
    }

    int ws_connect_main(const std::string& url,
                        bool disableAutomaticReconnection,
                        bool disablePerMessageDeflate)
    {
        std::cout << "Type Ctrl-D to exit prompt..." << std::endl;
        WebSocketConnect webSocketChat(url,
                                       disableAutomaticReconnection,
                                       disablePerMessageDeflate);
        webSocketChat.start();

        while (true)
        {
            // Read line
            std::string line;
            auto quit = linenoise::Readline("> ", line);

            if (quit)
            {
                break;
            }

            if (line == "/stop")
            {
                std::cout << "Stopping connection..." << std::endl;
                webSocketChat.stop();
                continue;
            }

            if (line == "/start")
            {
                std::cout << "Starting connection..." << std::endl;
                webSocketChat.start();
                continue;
            }

            webSocketChat.sendMessage(line);

            // Add text to history
            linenoise::AddHistory(line.c_str());
        }

        std::cout << std::endl;
        webSocketChat.stop();

        return 0;
    }
}

