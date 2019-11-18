/*
 *  ws_proxy_server.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <ixwebsocket/IXWebSocketServer.h>
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
        std::cout << "Listening on " << hostname << ":" << port << std::endl;

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
                    std::cerr << "New connection" << std::endl;
                    std::cerr << "server id: " << state->getId() << std::endl;
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
                              << " code " << msg->closeInfo.code << " reason "
                              << msg->closeInfo.reason << std::endl;
                    webSocket->close(msg->closeInfo.code, msg->closeInfo.reason);
                    state->setTerminated();
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
                    std::cerr << "Received " << msg->wireSize << " bytes from server" << std::endl;
                    if (verbose)
                    {
                        std::cerr << "payload " << msg->str << std::endl;
                    }

                    webSocket->send(msg->str, msg->binary);
                }
            });

            // Client connection
            webSocket->setOnMessageCallback([state, remoteUrl, verbose](
                                                const WebSocketMessagePtr& msg) {
                if (msg->type == ix::WebSocketMessageType::Open)
                {
                    std::cerr << "New connection" << std::endl;
                    std::cerr << "client id: " << state->getId() << std::endl;
                    std::cerr << "Uri: " << msg->openInfo.uri << std::endl;
                    std::cerr << "Headers:" << std::endl;
                    for (auto it : msg->openInfo.headers)
                    {
                        std::cerr << it.first << ": " << it.second << std::endl;
                    }

                    // Connect to the 'real' server
                    std::string url(remoteUrl);
                    url += msg->openInfo.uri;
                    state->webSocket().setUrl(url);
                    state->webSocket().start();

                    // we should sleep here for a bit until we've established the
                    // connection with the remote server
                    while (state->webSocket().getReadyState() != ReadyState::Open)
                    {
                        std::cerr << "waiting for server connection establishment" << std::endl;
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                    std::cerr << "server connection established" << std::endl;
                }
                else if (msg->type == ix::WebSocketMessageType::Close)
                {
                    std::cerr << "Closed connection"
                              << " code " << msg->closeInfo.code << " reason "
                              << msg->closeInfo.reason << std::endl;
                    state->webSocket().close(msg->closeInfo.code, msg->closeInfo.reason);
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
                    std::cerr << "Received " << msg->wireSize << " bytes from client" << std::endl;
                    if (verbose)
                    {
                        std::cerr << "payload " << msg->str << std::endl;
                    }
                    state->webSocket().send(msg->str, msg->binary);
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
