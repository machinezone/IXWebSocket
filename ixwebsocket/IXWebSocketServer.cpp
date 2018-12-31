/*
 *  IXWebSocketServer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocketServer.h"
#include "IXWebSocketTransport.h"
#include "IXWebSocket.h"

#include <sstream>
#include <iostream>

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
        server.sin_addr.s_addr = INADDR_ANY;
        // server.sin_addr.s_addr = INADDR_LOOPBACK;

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
            /*
             * Accept a connection.
             */
            struct sockaddr_in client; /* client address information */
            int clientFd;              /* socket connected to client */
            socklen_t addressLen = sizeof(socklen_t);

            if ((clientFd = accept(_serverFd, (struct sockaddr *)&client, &addressLen)) == -1)
            {
                std::cerr << "WebSocketServer::run() error accepting connection: "
                          << strerror(errno)
                          << std::endl;
                continue;
            }

            _workers.push_back(std::thread(&WebSocketServer::handleConnection, this, clientFd));
        }
    }

    void WebSocketServer::handleConnection(int fd)
    {
        ix::WebSocket webSocket;
        webSocket.setSocketFileDescriptor(fd);

        webSocket.setOnMessageCallback(
            [&webSocket](ix::WebSocketMessageType messageType,
               const std::string& str,
               size_t wireSize,
               const ix::WebSocketErrorInfo& error,
               const ix::WebSocketCloseInfo& closeInfo,
               const ix::WebSocketHttpHeaders& headers)
            {
                if (messageType == ix::WebSocket_MessageType_Message)
                {
                    std::cout << str << std::endl;
                    webSocket.send(str);
                }
            }
        );

        webSocket.start();

        for (;;)
        {
            std::chrono::duration<double, std::milli> wait(10);
            std::this_thread::sleep_for(wait);
        }
    }
}
