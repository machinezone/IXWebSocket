/*
 *  IXWebSocketServer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocketServer.h"

#include "IXNetSystem.h"
#include "IXSetThreadName.h"
#include "IXSocketConnect.h"
#include "IXWebSocket.h"
#include "IXWebSocketTransport.h"
#include <future>
#include <sstream>
#include <string.h>

namespace ix
{
    const int WebSocketServer::kDefaultHandShakeTimeoutSecs(3); // 3 seconds
    const bool WebSocketServer::kDefaultEnablePong(true);

    WebSocketServer::WebSocketServer(int port,
                                     const std::string& host,
                                     int backlog,
                                     size_t maxConnections,
                                     int handshakeTimeoutSecs,
                                     int addressFamily)
        : SocketServer(port, host, backlog, maxConnections, addressFamily)
        , _handshakeTimeoutSecs(handshakeTimeoutSecs)
        , _enablePong(kDefaultEnablePong)
        , _enablePerMessageDeflate(true)
    {
    }

    WebSocketServer::~WebSocketServer()
    {
        stop();
    }

    void WebSocketServer::stop()
    {
        stopAcceptingConnections();

        auto clients = getClients();
        for (auto client : clients)
        {
            client->close();
        }

        SocketServer::stop();
    }

    void WebSocketServer::enablePong()
    {
        _enablePong = true;
    }

    void WebSocketServer::disablePong()
    {
        _enablePong = false;
    }

    void WebSocketServer::disablePerMessageDeflate()
    {
        _enablePerMessageDeflate = false;
    }

    void WebSocketServer::setOnConnectionCallback(const OnConnectionCallback& callback)
    {
        _onConnectionCallback = callback;
    }

    void WebSocketServer::handleConnection(std::unique_ptr<Socket> socket,
                                           std::shared_ptr<ConnectionState> connectionState)
    {
        setThreadName("WebSocketServer::" + connectionState->getId());

        auto webSocket = std::make_shared<WebSocket>();
        _onConnectionCallback(webSocket, connectionState);

        webSocket->disableAutomaticReconnection();

        if (_enablePong)
        {
            webSocket->enablePong();
        }
        else
        {
            webSocket->disablePong();
        }

        // Add this client to our client set
        {
            std::lock_guard<std::mutex> lock(_clientsMutex);
            _clients.insert(webSocket);
        }

        auto status = webSocket->connectToSocket(std::move(socket), _handshakeTimeoutSecs);
        if (status.success)
        {
            // Process incoming messages and execute callbacks
            // until the connection is closed
            webSocket->run();
        }
        else
        {
            std::stringstream ss;
            ss << "WebSocketServer::handleConnection() HTTP status: " << status.http_status
               << " error: " << status.errorStr;
            logError(ss.str());
        }

        // Remove this client from our client set
        {
            std::lock_guard<std::mutex> lock(_clientsMutex);
            if (_clients.erase(webSocket) != 1)
            {
                logError("Cannot delete client");
            }
        }

        connectionState->setTerminated();
    }

    std::set<std::shared_ptr<WebSocket>> WebSocketServer::getClients()
    {
        std::lock_guard<std::mutex> lock(_clientsMutex);
        return _clients;
    }

    size_t WebSocketServer::getConnectedClientsCount()
    {
        std::lock_guard<std::mutex> lock(_clientsMutex);
        return _clients.size();
    }
} // namespace ix
