/*
 *  ws_connect.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXSocket.h>

using namespace ix;

namespace
{
    void log(const std::string& msg)
    {
        std::cout << msg << std::endl;
    }

    class WebSocketPingPong
    {
        public:
            WebSocketPingPong(const std::string& _url);

            void subscribe(const std::string& channel);
            void start();
            void stop();

            void ping(const std::string& text);

        private:
            std::string _url;
            ix::WebSocket _webSocket;
    };

    WebSocketPingPong::WebSocketPingPong(const std::string& url) :
        _url(url)
    {
        ;
    }

    void WebSocketPingPong::stop()
    {
        _webSocket.stop();
    }

    void WebSocketPingPong::start()
    {
        _webSocket.configure(_url);

        std::stringstream ss;
        log(std::string("Connecting to url: ") + _url);

        _webSocket.setOnMessageCallback(
            [this](ix::WebSocketMessageType messageType, const std::string& str, ix::WebSocketErrorInfo error)
            {
                std::stringstream ss;
                if (messageType == ix::WebSocket_MessageType_Open)
                {
                    log("ws_connect: connected");
                }
                else if (messageType == ix::WebSocket_MessageType_Close)
                {
                    log("ws_connect: disconnected");
                }
                else if (messageType == ix::WebSocket_MessageType_Message)
                {
                    ss << "ws_connect: received message: "
                       << str;
                    log(ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Ping)
                {
                    ss << "ws_connect: received ping message: "
                       << str;
                    log(ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Pong)
                {
                    ss << "ws_connect: received pong message: "
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
        _webSocket.ping(text);
    }

    void interactiveMain(const std::string& url)
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

            webSocketPingPong.ping(text);
        }

        std::cout << std::endl;
        webSocketPingPong.stop();
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ping_pong <url>" << std::endl;
        return 1;
    }
    std::string url = argv[1];

    Socket::init();
    interactiveMain(url);
    return 0;
}
