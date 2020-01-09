/*
 *  ws_broadcast_server.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include <ixwebsocket/IXWebSocketServer.h>
#include <spdlog/spdlog.h>
#include <sstream>


namespace ix
{
    int ws_broadcast_server_main(int port,
                                 const std::string& hostname,
                                 const ix::SocketTLSOptions& tlsOptions)
    {
        spdlog::info("Listening on {}:{}", hostname, port);

        ix::WebSocketServer server(port, hostname);
        server.setTLSOptions(tlsOptions);

        server.setOnConnectionCallback([&server](std::shared_ptr<WebSocket> webSocket,
                                                 std::shared_ptr<ConnectionState> connectionState) {
            webSocket->setOnMessageCallback([webSocket, connectionState, &server](
                                                const WebSocketMessagePtr& msg) {
                if (msg->type == ix::WebSocketMessageType::Open)
                {
                    spdlog::info("New connection");
                    spdlog::info("id: {}", connectionState->getId());
                    spdlog::info("Uri: {}", msg->openInfo.uri);
                    spdlog::info("Headers:");
                    for (auto it : msg->openInfo.headers)
                    {
                        spdlog::info("{}: {}", it.first, it.second);
                    }
                }
                else if (msg->type == ix::WebSocketMessageType::Close)
                {
                    spdlog::info("Closed connection: code {} reason {}",
                                 msg->closeInfo.code,
                                 msg->closeInfo.reason);
                }
                else if (msg->type == ix::WebSocketMessageType::Error)
                {
                    std::stringstream ss;
                    ss << "Connection error: " << msg->errorInfo.reason << std::endl;
                    ss << "#retries: " << msg->errorInfo.retries << std::endl;
                    ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                    ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                    spdlog::info(ss.str());
                }
                else if (msg->type == ix::WebSocketMessageType::Fragment)
                {
                    spdlog::info("Received message fragment");
                }
                else if (msg->type == ix::WebSocketMessageType::Message)
                {
                    spdlog::info("Received {} bytes", msg->wireSize);

                    for (auto&& client : server.getClients())
                    {
                        if (client != webSocket)
                        {
                            client->send(msg->str, msg->binary, [](int current, int total) -> bool {
                                spdlog::info("Step {} out of {}", current, total);
                                return true;
                            });

                            do
                            {
                                size_t bufferedAmount = client->bufferedAmount();
                                spdlog::info("{} bytes left to be sent", bufferedAmount);

                                std::chrono::duration<double, std::milli> duration(500);
                                std::this_thread::sleep_for(duration);
                            } while (client->bufferedAmount() != 0);
                        }
                    }
                }
            });
        });

        auto res = server.listen();
        if (!res.first)
        {
            spdlog::info(res.second);
            return 1;
        }

        server.start();
        server.wait();

        return 0;
    }
} // namespace ix
