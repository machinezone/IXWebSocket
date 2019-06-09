/*
 *  ws_transfer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <ixwebsocket/IXWebSocketServer.h>

namespace ix
{
    int ws_transfer_main(int port, const std::string& hostname)
    {
        std::cout << "Listening on " << hostname << ":" << port << std::endl;

        ix::WebSocketServer server(port, hostname);

        server.setOnConnectionCallback(
            [&server](std::shared_ptr<ix::WebSocket> webSocket,
                      std::shared_ptr<ConnectionState> connectionState)
            {
                webSocket->setOnMessageCallback(
                    [webSocket, connectionState, &server](const WebSocketMessagePtr& msg)
                    {
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
                        }
                        else if (msg->type == ix::WebSocketMessageType::Close)
                        {
                            std::cerr << "Closed connection"
                                      << " code " << msg->closeInfo.code
                                      << " reason " << msg->closeInfo.reason << std::endl;
                        }
                        else if (msg->type == ix::WebSocketMessageType::Error)
                        {
                            std::stringstream ss;
                            ss << "Connection error: " << msg->errorInfo.reason      << std::endl;
                            ss << "#retries: "         << msg->errorInfo.retries     << std::endl;
                            ss << "Wait time(ms): "    << msg->errorInfo.wait_time   << std::endl;
                            ss << "HTTP Status: "      << msg->errorInfo.http_status << std::endl;
                            std::cerr << ss.str();
                        }
                        else if (msg->type == ix::WebSocketMessageType::Fragment)
                        {
                            std::cerr << "Received message fragment "
                                      << std::endl;
                        }
                        else if (msg->type == ix::WebSocketMessageType::Message)
                        {
                            std::cerr << "Received " << msg->wireSize << " bytes" << std::endl;
                            for (auto&& client : server.getClients())
                            {
                                if (client != webSocket)
                                {
                                    client->send(msg->str,
                                                 msg->binary,
                                                 [](int current, int total) -> bool
                                    {
                                        std::cerr << "ws_transfer: Step " << current
                                                  << " out of " << total << std::endl;
                                        return true;
                                    });

                                    do
                                    {
                                        size_t bufferedAmount = client->bufferedAmount();
                                        std::cerr << "ws_transfer: " << bufferedAmount
                                                  << " bytes left to be sent" << std::endl;

                                        std::chrono::duration<double, std::milli> duration(10);
                                        std::this_thread::sleep_for(duration);
                                    } while (client->bufferedAmount() != 0);
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
            std::cerr << res.second << std::endl;
            return 1;
        }

        server.start();
        server.wait();

        return 0;
    }
}
