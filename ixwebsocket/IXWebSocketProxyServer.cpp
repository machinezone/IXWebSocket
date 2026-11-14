/*
 *  IXWebSocketProxyServer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocketProxyServer.h"

#include "IXWebSocketProxyServerPrivate.h"

namespace ix
{
    void detail::configureWebSocketProxyServer(WebSocketServer& server,
                                               const std::string& remoteUrl,
                                               const RemoteUrlsMapping& remoteUrlsMapping)
    {
        auto factory = []() -> std::shared_ptr<ix::ConnectionState>
        { return std::make_shared<ProxyConnectionState>(); };
        server.setConnectionStateFactory(factory);

        server.setOnConnectionCallback(
            [remoteUrl, remoteUrlsMapping](std::weak_ptr<ix::WebSocket> webSocket,
                                           std::shared_ptr<ConnectionState> connectionState)
            {
                auto state = std::static_pointer_cast<ProxyConnectionState>(connectionState);
                // The server owns state until its downstream worker exits. Retaining
                // it in the upstream callback would create a cycle through webSocket().
                state->webSocket().setOnMessageCallback(
                    [webSocket](const WebSocketMessagePtr& msg)
                    {
                        auto ws = webSocket.lock();
                        if (!ws) return;
                        if (msg->type == ix::WebSocketMessageType::Close)
                        {
                            ws->close(msg->closeInfo.code, msg->closeInfo.reason);
                        }
                        else if (msg->type == ix::WebSocketMessageType::Error)
                        {
                            ws->close(WebSocketCloseConstants::kInternalErrorCode,
                                      "Upstream connection failed");
                        }
                        else if (msg->type == ix::WebSocketMessageType::Message)
                        {
                            ws->send(msg->str, msg->binary);
                        }
                    });

                // Client connection
                auto ws = webSocket.lock();
                if (ws)
                {
                    ws->setOnMessageCallback(
                        [state, webSocket, remoteUrl, remoteUrlsMapping](
                            const WebSocketMessagePtr& msg)
                        {
                            auto downstream = webSocket.lock();
                            if (!downstream) return;
                            if (msg->type == ix::WebSocketMessageType::Open)
                            {
                                // Connect to the 'real' server
                                std::string url(remoteUrl);

                                // maybe we want a different url based on the mapping
                                std::string host = msg->openInfo.headers["Host"];
                                auto it = remoteUrlsMapping.find(host);
                                if (it != remoteUrlsMapping.end())
                                {
                                    url = it->second;
                                }

                                // append the uri to form the full url
                                // (say ws://localhost:1234/foo/?bar=baz)
                                url += msg->openInfo.uri;

                                state->webSocket().setUrl(url);
                                state->webSocket().disableAutomaticReconnection();
                                state->webSocket().start();

                                // Stop waiting if upstream setup fails or the server shuts
                                // down.
                                while (state->webSocket().getReadyState() != ReadyState::Open &&
                                       downstream->getReadyState() == ReadyState::Open)
                                {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                }
                            }
                            else if (msg->type == ix::WebSocketMessageType::Close)
                            {
                                state->webSocket().close(msg->closeInfo.code,
                                                         msg->closeInfo.reason);
                            }
                            else if (msg->type == ix::WebSocketMessageType::Message)
                            {
                                state->webSocket().send(msg->str, msg->binary);
                            }
                        });
                }
            });
    }

    int websocket_proxy_server_main(int port,
                                    const std::string& hostname,
                                    const ix::SocketTLSOptions& tlsOptions,
                                    const std::string& remoteUrl,
                                    const RemoteUrlsMapping& remoteUrlsMapping,
                                    bool /*verbose*/)
    {
        ix::WebSocketServer server(port, hostname);
        server.setTLSOptions(tlsOptions);

        detail::configureWebSocketProxyServer(server, remoteUrl, remoteUrlsMapping);

        auto res = server.listen();
        if (!res.first)
        {
            return 1;
        }

        server.start();
        server.wait();

        return 0;
    }
} // namespace ix
