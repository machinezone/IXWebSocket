/*
 *  ws_ping_pong.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018-2019 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXSocketTLSOptions.h>
#include <ixwebsocket/IXWebSocket.h>
#include <spdlog/spdlog.h>
#include <sstream>

namespace ix
{
    class WebSocketPingPong
    {
    public:
        WebSocketPingPong(const std::string& _url, const ix::SocketTLSOptions& tlsOptions);

        void subscribe(const std::string& channel);
        void start();
        void stop();

        void ping(const std::string& text);
        void send(const std::string& text);

    private:
        std::string _url;
        ix::WebSocket _webSocket;

        void log(const std::string& msg);
    };

    WebSocketPingPong::WebSocketPingPong(const std::string& url,
                                         const ix::SocketTLSOptions& tlsOptions)
        : _url(url)
    {
        _webSocket.setTLSOptions(tlsOptions);
    }

    void WebSocketPingPong::log(const std::string& msg)
    {
        spdlog::info(msg);
    }

    void WebSocketPingPong::stop()
    {
        _webSocket.stop();
    }

    void WebSocketPingPong::start()
    {
        _webSocket.setUrl(_url);

        std::stringstream ss;
        log(std::string("Connecting to url: ") + _url);

        _webSocket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
            spdlog::info("Received {} bytes", msg->wireSize);

            std::stringstream ss;
            if (msg->type == ix::WebSocketMessageType::Open)
            {
                log("ping_pong: connected");

                spdlog::info("Uri: {}", msg->openInfo.uri);
                spdlog::info("Headers:");
                for (auto it : msg->openInfo.headers)
                {
                    spdlog::info("{}: {}", it.first, it.second);
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                ss << "ping_pong: disconnected:"
                   << " code " << msg->closeInfo.code << " reason " << msg->closeInfo.reason
                   << msg->str;
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Message)
            {
                ss << "ping_pong: received message: " << msg->str;
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Ping)
            {
                ss << "ping_pong: received ping message: " << msg->str;
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Pong)
            {
                ss << "ping_pong: received pong message: " << msg->str;
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

    void WebSocketPingPong::ping(const std::string& text)
    {
        if (!_webSocket.ping(text).success)
        {
            std::cerr << "Failed to send ping message. Message too long (> 125 bytes) or endpoint "
                         "is disconnected"
                      << std::endl;
        }
    }

    void WebSocketPingPong::send(const std::string& text)
    {
        _webSocket.send(text);
    }

    int ws_ping_pong_main(const std::string& url, const ix::SocketTLSOptions& tlsOptions)
    {
        spdlog::info("Type Ctrl-D to exit prompt...");
        WebSocketPingPong webSocketPingPong(url, tlsOptions);
        webSocketPingPong.start();

        while (true)
        {
            std::string text;
            std::cout << "> " << std::flush;
            std::getline(std::cin, text);

            if (!std::cin)
            {
                break;
            }

            if (text == "/close")
            {
                webSocketPingPong.send(text);
            }
            else
            {
                webSocketPingPong.ping(text);
            }
        }

        std::cout << std::endl;
        webSocketPingPong.stop();

        return 0;
    }
} // namespace ix
