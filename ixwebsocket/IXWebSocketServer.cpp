/*
 *  IXWebSocketServer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocketServer.h"
#include "IXWebSocketTransport.h"
#include "IXWebSocket.h"

#include <sstream>

#include <netdb.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>

namespace ix 
{
    WebSocketServer::WebSocketServer(int port, int backlog) :
        _port(port),
        _backlog(backlog)
    {

    }

    WebSocketServer::~WebSocketServer()
    {

    }

    void WebSocketServer::setOnConnectionCallback(const OnConnectionCallback& callback)
    {
        _onConnectionCallback = callback;
    }

    std::pair<bool, std::string> WebSocketServer::listen()
    {
        struct sockaddr_in server; /* server address information          */

        /*
         * Get a socket for accepting connections.
         */
        if ((_serverFd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            std::stringstream ss;
            ss << "WebSocketServer::listen() error creating socket): "
               << strerror(errno);

            return std::make_pair(false, ss.str());
        }

        int enable = 1;
        if (setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        {
            std::stringstream ss;
            ss << "WebSocketServer::listen() error calling setsockopt(SO_REUSEADDR): "
               << strerror(errno);

            return std::make_pair(false, ss.str());
        }

        /*
         * Bind the socket to the server address.
         */
        server.sin_family = AF_INET;
        server.sin_port   = htons(_port);

        // Using INADDR_ANY trigger a pop-up box as binding to any address is detected 
        // by the osx firewall. We need to codesign the binary with a self-signed cert
        // to allow that, but this is a bit of a pain. (this is what node or python would do).
        //
        // Using INADDR_LOOPBACK also does not work ... while it should.
        //
        server.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (bind(_serverFd, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
            std::stringstream ss;
            ss << "WebSocketServer::listen() error calling bind: "
               << strerror(errno);

            return std::make_pair(false, ss.str());
        }

        /*
         * Listen for connections. Specify the tcp backlog.
         */
        if (::listen(_serverFd, _backlog) != 0)
        {
            std::stringstream ss;
            ss << "WebSocketServer::listen() error calling listen: "
               << strerror(errno);

            return std::make_pair(false, ss.str());
        }

        return std::make_pair(true, "");
    }

    void WebSocketServer::run()
    {
        for (;;)
        {
            // Accept a connection.
            struct sockaddr_in client; // client address information
            int clientFd;              // socket connected to client
            socklen_t addressLen = sizeof(socklen_t);

            if ((clientFd = accept(_serverFd, (struct sockaddr *)&client, &addressLen)) == -1)
            {
                std::cerr << "WebSocketServer::run() error accepting connection: "
                    << strerror(errno);
                continue;
            }

            _workers.push_back(std::thread(&WebSocketServer::handleConnection, this, clientFd));
        }
    }

    //
    // FIXME: make sure we never run into reconnectPerpetuallyIfDisconnected
    //
    void WebSocketServer::handleConnection(int fd)
    {
        ix::WebSocket webSocket;
        _onConnectionCallback(webSocket);

        webSocket.start();
        webSocket.connectToSocket(fd); // FIXME: we ignore the return value

        // We can probably do better than this busy loop, with a condition variable.
        for (;;)
        {
            std::chrono::duration<double, std::milli> wait(10);
            std::this_thread::sleep_for(wait);
        }
    }
}
