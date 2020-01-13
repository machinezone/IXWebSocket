/*
 *  ws_transfer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include <ixwebsocket/IXWebSocketServer.h>
#include <spdlog/spdlog.h>
#include <sstream>

namespace ix
{
    int ws_transfer_main(int port,
                         const std::string& hostname,
                         const ix::SocketTLSOptions& tlsOptions)
    {
        spdlog::info("Listening on {}:{}", hostname, port);

        ix::WebSocketServer server(port, hostname);
        server.setTLSOptions(tlsOptions);

        server.setOnConnectionCallback([&server](std::shared_ptr<ix::WebSocket> webSocket,
                                                 std::shared_ptr<ConnectionState> connectionState) {
            webSocket->setOnMessageCallback([webSocket, connectionState, &server](
                                                const WebSocketMessagePtr& msg) {
                if (msg->type == ix::WebSocketMessageType::Open)
                {
                    spdlog::info("ws_transfer: New connection");
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
                    spdlog::info("ws_transfer: Closed connection: client id {} code {} reason {}",
                                 connectionState->getId(),
                                 msg->closeInfo.code,
                                 msg->closeInfo.reason);
                    auto remaining = server.getClients().erase(webSocket);
                    spdlog::info("ws_transfer: {} remaining clients", remaining);
                }
                else if (msg->type == ix::WebSocketMessageType::Error)
                {
                    std::stringstream ss;
                    ss << "ws_transfer: Connection error: " << msg->errorInfo.reason << std::endl;
                    ss << "#retries: " << msg->errorInfo.retries << std::endl;
                    ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                    ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                    spdlog::info(ss.str());
                }
                else if (msg->type == ix::WebSocketMessageType::Fragment)
                {
                    spdlog::info("ws_transfer: Received message fragment ");
                }
                else if (msg->type == ix::WebSocketMessageType::Message)
                {
                    spdlog::info("ws_transfer: Received {} bytes", msg->wireSize);
                    size_t receivers = 0;
                    for (auto&& client : server.getClients())
                    {
                        if (client != webSocket)
                        {
                            auto readyState = client->getReadyState();
                            auto id = connectionState->getId();

                            if (readyState == ReadyState::Open)
                            {
                                ++receivers;
                                client->send(
                                    msg->str, msg->binary, [&id](int current, int total) -> bool {
                                        spdlog::info("{}: [client {}]: Step {} out of {}",
                                                     "ws_transfer",
                                                     id,
                                                     current,
                                                     total);
                                        return true;
                                    });
                                do
                                {
                                    size_t bufferedAmount = client->bufferedAmount();

                                    spdlog::info("{}: [client {}]: {} bytes left to send",
                                                 "ws_transfer",
                                                 id,
                                                 bufferedAmount);

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

                                spdlog::info(
                                    "{}: [client {}]: has readystate {} bytes left to be sent {}",
                                    "ws_transfer",
                                    id,
                                    readyStateString,
                                    bufferedAmount);
                            }
                        }
                    }
                    if (!receivers)
                    {
                        spdlog::info("ws_transfer: no remaining receivers");
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
