/*
 *  IXHttpServer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXHttpServer.h"
#include "IXSocketConnect.h"
#include "IXSocketFactory.h"
#include "IXNetSystem.h"

#include <iostream>
#include <sstream>
#include <future>
#include <string.h>

namespace ix
{
    const int HttpServer::kDefaultHandShakeTimeoutSecs(3); // 3 seconds

    HttpServer::HttpServer(int port,
                           const std::string& host,
                           int backlog,
                           size_t maxConnections,
                           int handshakeTimeoutSecs) : SocketServer(port, host, backlog, maxConnections),
        _handshakeTimeoutSecs(handshakeTimeoutSecs),
        _connectedClientsCount(0)
    {

    }

    HttpServer::~HttpServer()
    {
        stop();
    }

    void HttpServer::stop()
    {
        stopAcceptingConnections();

        // FIXME: cancelling / closing active clients ...

        SocketServer::stop();
    }

    void HttpServer::setOnConnectionCallback(const OnConnectionCallback& callback)
    {
        _onConnectionCallback = callback;
    }

    void HttpServer::handleConnection(
        int fd,
        std::shared_ptr<ConnectionState> connectionState)
    {
        _connectedClientsCount++;

        std::string errorMsg;
        auto socket = createSocket(fd, errorMsg);
        // Set the socket to non blocking mode + other tweaks
        SocketConnect::configure(fd);

        auto ret = Http::parseRequest(socket);
        // FIXME: handle errors in parseRequest

        auto response = _onConnectionCallback(std::get<2>(ret), connectionState);
        Http::sendResponse(response, socket);

        logInfo("HttpServer::handleConnection() done");
        connectionState->setTerminated();

        _connectedClientsCount--;
    }

    size_t HttpServer::getConnectedClientsCount()
    {
        return _connectedClientsCount;
    }
}
