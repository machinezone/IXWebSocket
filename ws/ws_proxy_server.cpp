/*
 *  ws_proxy_server.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include <ixwebsocket/IXWebSocketServer.h>
#include <spdlog/spdlog.h>
#include <sstream>

namespace ix
{
    class ProxyConnectionState : public ix::ConnectionState
    {
    public:
        ProxyConnectionState()
            : _connected(false)
        {
        }

        ix::WebSocket& webSocket()
        {
            return _serverWebSocket;
        }

        bool isConnected()
        {
            return _connected;
        }

        void setConnected()
        {
            _connected = true;
        }

    private:
        ix::WebSocket _serverWebSocket;
        bool _connected;
    };

    int ws_proxy_server_main(int port,
                             const std::string& hostname,
                             const ix::SocketTLSOptions& tlsOptions,
                             const std::string& remoteUrl,
                             bool verbose)
    {
        spdlog::info("Listening on {}:{}", hostname, port);

        ix::WebSocketServer server(port, hostname);
        server.setTLSOptions(tlsOptions);

        auto factory = []() -> std::shared_ptr<ix::ConnectionState> {
            return std::make_shared<ProxyConnectionState>();
        };
        server.setConnectionStateFactory(factory);

        server.setOnConnectionCallback([remoteUrl,
                                        verbose](std::shared_ptr<ix::WebSocket> webSocket,
                                                 std::shared_ptr<ConnectionState> connectionState) {
            auto state = std::dynamic_pointer_cast<ProxyConnectionState>(connectionState);

            // Server connection
            state->webSocket().setOnMessageCallback([webSocket, state, verbose](
                                                        const WebSocketMessagePtr& msg) {
                if (msg->type == ix::WebSocketMessageType::Open)
                {
                    spdlog::info("New connection to remote server");
                    spdlog::info("id: {}", state->getId());
                    spdlog::info("Uri: {}", msg->openInfo.uri);
                    spdlog::info("Headers:");
                    for (auto it : msg->openInfo.headers)
                    {
                        spdlog::info("{}: {}", it.first, it.second);
                    }
                }
                else if (msg->type == ix::WebSocketMessageType::Close)
                {
                    spdlog::info("Closed remote server connection: client id {} code {} reason {}",
                                 state->getId(),
                                 msg->closeInfo.code,
                                 msg->closeInfo.reason);
                    state->setTerminated();
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
                    spdlog::info("Received {} bytes from server", msg->wireSize);
                    if (verbose)
                    {
                        spdlog::info("payload {}", msg->str);
                    }

                    webSocket->send(msg->str, msg->binary);
                }
            });

            // Client connection
            webSocket->setOnMessageCallback(
                [state, remoteUrl, verbose](const WebSocketMessagePtr& msg) {
                    if (msg->type == ix::WebSocketMessageType::Open)
                    {
                        spdlog::info("New connection from client");
                        spdlog::info("id: {}", state->getId());
                        spdlog::info("Uri: {}", msg->openInfo.uri);
                        spdlog::info("Headers:");
                        for (auto it : msg->openInfo.headers)
                        {
                            spdlog::info("{}: {}", it.first, it.second);
                        }

                        // Connect to the 'real' server
                        std::string url(remoteUrl);
                        url += msg->openInfo.uri;
                        state->webSocket().setUrl(url);
                        state->webSocket().disableAutomaticReconnection();
                        state->webSocket().start();

                        // we should sleep here for a bit until we've established the
                        // connection with the remote server
                        while (state->webSocket().getReadyState() != ReadyState::Open)
                        {
                            spdlog::info("waiting for server connection establishment");
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        }
                        spdlog::info("server connection established");
                    }
                    else if (msg->type == ix::WebSocketMessageType::Close)
                    {
                        spdlog::info("Closed client connection: client id {} code {} reason {}",
                                     state->getId(),
                                     msg->closeInfo.code,
                                     msg->closeInfo.reason);
                        state->webSocket().close(msg->closeInfo.code, msg->closeInfo.reason);
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
                        spdlog::info("Received {} bytes from client", msg->wireSize);
                        if (verbose)
                        {
                            spdlog::info("payload {}", msg->str);
                        }

                        state->webSocket().send(msg->str, msg->binary);
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
