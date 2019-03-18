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
            [&server](std::shared_ptr<ix::WebSocket> webSocket)
            {
                webSocket->setOnMessageCallback(
                    [webSocket, &server](ix::WebSocketMessageType messageType,
                       const std::string& str,
                       size_t wireSize,
                       const ix::WebSocketErrorInfo& error,
                       const ix::WebSocketOpenInfo& openInfo,
                       const ix::WebSocketCloseInfo& closeInfo)
                    {
                        if (messageType == ix::WebSocket_MessageType_Open)
                        {
                            std::cerr << "New connection" << std::endl;
                            std::cerr << "Uri: " << openInfo.uri << std::endl;
                            std::cerr << "Headers:" << std::endl;
                            for (auto it : openInfo.headers)
                            {
                                std::cerr << it.first << ": " << it.second << std::endl;
                            }
                        }
                        else if (messageType == ix::WebSocket_MessageType_Close)
                        {
                            std::cerr << "Closed connection"
                                      << " code " << closeInfo.code
                                      << " reason " << closeInfo.reason << std::endl;
                        }
                        else if (messageType == ix::WebSocket_MessageType_Error)
                        {
                            std::stringstream ss;
                            ss << "Connection error: " << error.reason      << std::endl;
                            ss << "#retries: "         << error.retries     << std::endl;
                            ss << "Wait time(ms): "    << error.wait_time   << std::endl;
                            ss << "HTTP Status: "      << error.http_status << std::endl;
                            std::cerr << ss.str();
                        }
                        else if (messageType == ix::WebSocket_MessageType_Fragment)
                        {
                            std::cerr << "Received message fragment "
                                      << std::endl;
                        }
                        else if (messageType == ix::WebSocket_MessageType_Message)
                        {
                            std::cerr << "Received " << wireSize << " bytes" << std::endl;
                            for (auto&& client : server.getClients())
                            {
                                if (client != webSocket)
                                {
                                    client->send(str,
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
