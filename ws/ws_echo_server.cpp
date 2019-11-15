/*
 *  ws_echo_server.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <ixwebsocket/IXWebSocketServer.h>
#include <sstream>

namespace ix
{
    int ws_echo_server_main(int port,
                            bool greetings,
                            const std::string& hostname,
                            const ix::SocketTLSOptions& tlsOptions)
    {
        std::cout << "Listening on " << hostname << ":" << port << std::endl;

        ix::WebSocketServer server(port, hostname);
        server.setTLSOptions(tlsOptions);

        server.setOnConnectionCallback(
            [greetings](std::shared_ptr<ix::WebSocket> webSocket,
                        std::shared_ptr<ConnectionState> connectionState) {
                webSocket->setOnMessageCallback(
                    [webSocket, connectionState, greetings](const WebSocketMessagePtr& msg) {
                        if (msg->type == ix::WebSocketMessageType::Open)
                        {
                            std::cerr << "New connection" << std::endl;
                            std::cerr << "id: " << connectionState->getId() << std::endl;
                            std::cerr << "Uri: " << msg->openInfo.uri << std::endl;
                            std::cerr << "Headers:" << std::endl;
                            for (auto it : msg->openInfo.headers)
                            {
                                std::cerr << it.first << ": " << it.second << std::endl;
                            }

                            if (greetings)
                            {
                                webSocket->sendText("Welcome !");
                            }
                        }
                        else if (msg->type == ix::WebSocketMessageType::Close)
                        {
                            std::cerr << "Closed connection"
                                      << " code " << msg->closeInfo.code << " reason "
                                      << msg->closeInfo.reason << std::endl;
                        }
                        else if (msg->type == ix::WebSocketMessageType::Error)
                        {
                            std::stringstream ss;
                            ss << "Connection error: " << msg->errorInfo.reason << std::endl;
                            ss << "#retries: " << msg->errorInfo.retries << std::endl;
                            ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                            ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                            std::cerr << ss.str();
                        }
                        else if (msg->type == ix::WebSocketMessageType::Message)
                        {
                            std::cerr << "Received " << msg->wireSize << " bytes" << std::endl;
                            webSocket->send(msg->str, msg->binary);
                        }
                    });
            });

        auto res = server.listen();
        if (!res.first)
        {
            std::cerr << res.second << std::endl;
            return 1;
        }

        server.start();
        server.wait();

        return 0;
    }
} // namespace ix
