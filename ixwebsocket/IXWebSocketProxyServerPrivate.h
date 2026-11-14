/*
 *  IXWebSocketProxyServerPrivate.h
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#pragma once

// Internal proxy setup shared by the command-line entry point and regression tests.
// This header is not installed as part of the public API.
#include "IXWebSocketProxyServer.h"
#include "IXWebSocketServer.h"

namespace ix
{
    namespace detail
    {
        class ProxyConnectionState : public ConnectionState
        {
        public:
            WebSocket& webSocket()
            {
                return _serverWebSocket;
            }

        private:
            WebSocket _serverWebSocket;
        };

        void configureWebSocketProxyServer(WebSocketServer& server,
                                           const std::string& remoteUrl,
                                           const RemoteUrlsMapping& remoteUrlsMapping);
    } // namespace detail
} // namespace ix
