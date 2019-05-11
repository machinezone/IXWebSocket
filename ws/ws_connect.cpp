/*
 *  ws_connect.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXSocket.h>

namespace ix
{
    class WebSocketConnect
    {
        public:
            WebSocketConnect(const std::string& _url,
                             bool disableAutomaticReconnection);

            void subscribe(const std::string& channel);
            void start();
            void stop();

            void sendMessage(const std::string& text);

        private:
            std::string _url;
            ix::WebSocket _webSocket;

            void log(const std::string& msg);
    };

    WebSocketConnect::WebSocketConnect(const std::string& url,
                                       bool disableAutomaticReconnection) :
        _url(url)
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

        ix::WebSocketPerMessageDeflateOptions webSocketPerMessageDeflateOptions(
            true, false, false, 15, 15);
        _webSocket.setPerMessageDeflateOptions(webSocketPerMessageDeflateOptions);

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
                if (messageType == ix::WebSocket_MessageType_Open)
                {
                    log("ws_connect: connected");
                    std::cout << "Uri: " << openInfo.uri << std::endl;
                    std::cout << "Handshake Headers:" << std::endl;
                    for (auto it : openInfo.headers)
                    {
                        std::cout << it.first << ": " << it.second << std::endl;
                    }
                }
                else if (messageType == ix::WebSocket_MessageType_Close)
                {
                    ss << "ws_connect: connection closed:";
                    ss << " code " << closeInfo.code;
                    ss << " reason " << closeInfo.reason << std::endl;
                    log(ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Message)
                {
                    std::cerr << "Received " << wireSize << " bytes" << std::endl;

                    ss << "ws_connect: received message: "
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
                else if (messageType == ix::WebSocket_MessageType_Fragment)
                {
                    std::cerr << "Received message fragment" << std::endl;
                }
                else if (messageType == ix::WebSocket_MessageType_Ping)
                {
                    std::cerr << "Received ping" << std::endl;
                }
                else if (messageType == ix::WebSocket_MessageType_Pong)
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
        _webSocket.send(text);
    }

    int ws_connect_main(const std::string& url, bool disableAutomaticReconnection)
    {
        std::cout << "Type Ctrl-D to exit prompt..." << std::endl;
        WebSocketConnect webSocketChat(url, disableAutomaticReconnection);
        webSocketChat.start();

        while (true)
        {
            std::string text;
            std::cout << "> " << std::flush;
            std::getline(std::cin, text);

            if (text == "/stop")
            {
                std::cout << "Stopping connection..." << std::endl;
                webSocketChat.stop();
                continue;
            }

            if (text == "/start")
            {
                std::cout << "Starting connection..." << std::endl;
                webSocketChat.start();
                continue;
            }

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

