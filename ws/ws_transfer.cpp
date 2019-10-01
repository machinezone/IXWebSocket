/*
 *  ws_transfer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <ixwebsocket/IXWebSocketServer.h>
#include <sstream>

namespace ix
{
    int ws_transfer_main(int port,
                         const std::string& hostname,
                         const ix::SocketTLSOptions& tlsOptions)
    {
        std::cout << "ws_transfer: Listening on " << hostname << ":" << port << std::endl;

        ix::WebSocketServer server(port, hostname);
        server.setTLSOptions(tlsOptions);

        server.setOnConnectionCallback([&server](std::shared_ptr<ix::WebSocket> webSocket,
                                                 std::shared_ptr<ConnectionState> connectionState) {
            webSocket->setOnMessageCallback([webSocket, connectionState, &server](
                                                const WebSocketMessagePtr& msg) {
                if (msg->type == ix::WebSocketMessageType::Open)
                {
                    std::cerr << "ws_transfer: New connection" << std::endl;
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
                    std::cerr << "ws_transfer: [client " << connectionState->getId()
                              << "]: Closed connection, code " << msg->closeInfo.code << " reason "
                              << msg->closeInfo.reason << std::endl;
                    auto remaining = server.getClients().erase(webSocket);
                    std::cerr << "ws_transfer: " << remaining << " remaining clients " << std::endl;
                }
                else if (msg->type == ix::WebSocketMessageType::Error)
                {
                    std::stringstream ss;
                    ss << "ws_transfer: Connection error: " << msg->errorInfo.reason << std::endl;
                    ss << "#retries: " << msg->errorInfo.retries << std::endl;
                    ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                    ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                    std::cerr << ss.str();
                }
                else if (msg->type == ix::WebSocketMessageType::Fragment)
                {
                    std::cerr << "ws_transfer: Received message fragment " << std::endl;
                }
                else if (msg->type == ix::WebSocketMessageType::Message)
                {
                    std::cerr << "ws_transfer: Received " << msg->wireSize << " bytes" << std::endl;
                    size_t receivers = 0;
                    for (auto&& client : server.getClients())
                    {
                        if (client != webSocket)
                        {
                            auto readyState = client->getReadyState();
                            if (readyState == ReadyState::Open)
                            {
                                ++receivers;
                                client->send(msg->str,
                                             msg->binary,
                                             [id = connectionState->getId()](int current,
                                                                             int total) -> bool {
                                                 std::cerr << "ws_transfer: [client " << id
                                                           << "]: Step " << current << " out of "
                                                           << total << std::endl;
                                                 return true;
                                             });

                                do
                                {
                                    size_t bufferedAmount = client->bufferedAmount();
                                    std::cerr << "ws_transfer: [client " << connectionState->getId()
                                              << "]: " << bufferedAmount
                                              << " bytes left to be sent, " << std::endl;

                                    std::this_thread::sleep_for(std::chrono::milliseconds(500));

                                } while (client->bufferedAmount() != 0 &&
                                         client->getReadyState() == ReadyState::Open);
                            }
                            else
                            {
                                std::string readyStateString =
                                    readyState == ReadyState::Connecting
                                        ? "Connecting"
                                        : readyState == ReadyState::Closing ? "Closing" : "Closed";
                                size_t bufferedAmount = client->bufferedAmount();
                                std::cerr << "ws_transfer: [client " << connectionState->getId()
                                          << "]: has readystate '" << readyStateString << "' and "
                                          << bufferedAmount << " bytes left to be sent, "
                                          << std::endl;
                            }
                        }
                    }
                    if (!receivers)
                    {
                        std::cerr << "ws_transfer: no remaining receivers" << std::endl;
                    }
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
