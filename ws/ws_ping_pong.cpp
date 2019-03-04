/*
 *  ws_ping_pong.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018-2019 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXSocket.h>

namespace ix
{
    class WebSocketPingPong
    {
        public:
            WebSocketPingPong(const std::string& _url);

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

    WebSocketPingPong::WebSocketPingPong(const std::string& url) :
        _url(url)
    {
        ;
    }

    void WebSocketPingPong::log(const std::string& msg)
    {
        std::cout << msg << std::endl;
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

        _webSocket.setOnMessageCallback(
            [this](ix::WebSocketMessageType messageType,
               const std::string& str,
               size_t wireSize,
               const ix::WebSocketErrorInfo& error,
               const ix::WebSocketOpenInfo& openInfo,
               const ix::WebSocketCloseInfo& closeInfo)
            {
                std::cerr << "Received " << wireSize << " bytes" << std::endl;

                std::stringstream ss;
                if (messageType == ix::WebSocket_MessageType_Open)
                {
                    log("ping_pong: connected");

                    std::cout << "Uri: " << openInfo.uri << std::endl;
                    std::cout << "Handshake Headers:" << std::endl;
                    for (auto it : openInfo.headers)
                    {
                        std::cout << it.first << ": " << it.second << std::endl;
                    }
                }
                else if (messageType == ix::WebSocket_MessageType_Close)
                {
                    ss << "ping_pong: disconnected:"
                       << " code " << closeInfo.code
                       << " reason " << closeInfo.reason
                       << str;
                    log(ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Message)
                {
                    ss << "ping_pong: received message: "
                       << str;
                    log(ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Ping)
                {
                    ss << "ping_pong: received ping message: "
                       << str;
                    log(ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Pong)
                {
                    ss << "ping_pong: received pong message: "
                       << str;
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

    void WebSocketPingPong::ping(const std::string& text)
    {
        if (!_webSocket.ping(text).success)
        {
            std::cerr << "Failed to send ping message. Message too long (> 125 bytes) or endpoint is disconnected"
                      << std::endl;
        }
    }

    void WebSocketPingPong::send(const std::string& text)
    {
        _webSocket.send(text);
    }

    int ws_ping_pong_main(const std::string& url)
    {
        std::cout << "Type Ctrl-D to exit prompt..." << std::endl;
        WebSocketPingPong webSocketPingPong(url);
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
}
