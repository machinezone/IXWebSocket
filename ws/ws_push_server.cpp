/*
 *  ws_push_server.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <spdlog/spdlog.h>
#include <sstream>

namespace ix
{
    int ws_push_server(int port,
                       bool greetings,
                       const std::string& hostname,
                       const ix::SocketTLSOptions& tlsOptions,
                       bool ipv6,
                       bool disablePerMessageDeflate,
                       bool disablePong,
                       const std::string& sendMsg)
    {
        spdlog::info("Listening on {}:{}", hostname, port);

        ix::WebSocketServer server(port,
                                   hostname,
                                   SocketServer::kDefaultTcpBacklog,
                                   SocketServer::kDefaultMaxConnections,
                                   WebSocketServer::kDefaultHandShakeTimeoutSecs,
                                   (ipv6) ? AF_INET6 : AF_INET);

        server.setTLSOptions(tlsOptions);

        if (disablePerMessageDeflate)
        {
            spdlog::info("Disable per message deflate");
            server.disablePerMessageDeflate();
        }

        if (disablePong)
        {
            spdlog::info("Disable responding to PING messages with PONG");
            server.disablePong();
        }

        server.setOnClientMessageCallback(
            [greetings, &sendMsg](std::shared_ptr<ConnectionState> connectionState,
                                  ConnectionInfo& connectionInfo,
                                  WebSocket& webSocket,
                                  const WebSocketMessagePtr& msg) {
                auto remoteIp = connectionInfo.remoteIp;
                if (msg->type == ix::WebSocketMessageType::Open)
                {
                    spdlog::info("New connection");
                    spdlog::info("remote ip: {}", remoteIp);
                    spdlog::info("id: {}", connectionState->getId());
                    spdlog::info("Uri: {}", msg->openInfo.uri);
                    spdlog::info("Headers:");
                    for (auto it : msg->openInfo.headers)
                    {
                        spdlog::info("{}: {}", it.first, it.second);
                    }

                    if (greetings)
                    {
                        webSocket.sendText("Welcome !");
                    }

                    bool binary = false;
                    while (true)
                    {
                        webSocket.send(sendMsg, binary);
                    }
                }
                else if (msg->type == ix::WebSocketMessageType::Close)
                {
                    spdlog::info("Closed connection: client id {} code {} reason {}",
                                 connectionState->getId(),
                                 msg->closeInfo.code,
                                 msg->closeInfo.reason);
                }
                else if (msg->type == ix::WebSocketMessageType::Error)
                {
                    spdlog::error("Connection error: {}", msg->errorInfo.reason);
                    spdlog::error("#retries: {}", msg->errorInfo.retries);
                    spdlog::error("Wait time(ms): {}", msg->errorInfo.wait_time);
                    spdlog::error("HTTP Status: {}", msg->errorInfo.http_status);
                }
                else if (msg->type == ix::WebSocketMessageType::Message)
                {
                    spdlog::info("Received {} bytes", msg->wireSize);
                    webSocket.send(msg->str, msg->binary);
                }
            });

        auto res = server.listen();
        if (!res.first)
        {
            spdlog::error(res.second);
            return 1;
        }

        server.start();
        server.wait();

        return 0;
    }
} // namespace ix
