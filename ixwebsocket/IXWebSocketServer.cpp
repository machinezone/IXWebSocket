/*
 *  IXWebSocketServer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocketServer.h"
#include "IXWebSocketTransport.h"
#include "IXWebSocket.h"
#include "IXSocketConnect.h"
#include "IXNetSystem.h"

#include <sstream>
#include <future>
#include <string.h>

namespace ix
{
    const int WebSocketServer::kDefaultHandShakeTimeoutSecs(3); // 3 seconds
    const bool WebSocketServer::kDefaultEnablePong(true);

    WebSocketServer::WebSocketServer(int port,
                                     const std::string& host,
                                     int backlog,
                                     size_t maxConnections,
                                     int handshakeTimeoutSecs) : SocketServer(port, host, backlog, maxConnections),
        _handshakeTimeoutSecs(handshakeTimeoutSecs),
        _enablePong(kDefaultEnablePong)
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

    void WebSocketServer::setOnConnectionCallback(const OnConnectionCallback& callback)
    {
        _onConnectionCallback = callback;
    }

    void WebSocketServer::handleConnection(
        int fd,
        std::shared_ptr<ConnectionState> connectionState)
    {
        auto webSocket = std::make_shared<WebSocket>();
        _onConnectionCallback(webSocket, connectionState);

        webSocket->disableAutomaticReconnection();

        if (_enablePong)
            webSocket->enablePong();
        else
            webSocket->disablePong();

        // Add this client to our client set
        {
            std::lock_guard<std::mutex> lock(_clientsMutex);
            _clients.insert(webSocket);
        }

        auto status = webSocket->connectToSocket(fd, _handshakeTimeoutSecs);
        if (status.success)
        {
            // Process incoming messages and execute callbacks
            // until the connection is closed
            webSocket->run();
        }
        else
        {
            std::stringstream ss;
            ss << "WebSocketServer::handleConnection() error: "
               << status.http_status
               << " error: "
               << status.errorStr;
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

        logInfo("WebSocketServer::handleConnection() done");
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
}
