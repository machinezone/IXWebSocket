/*
 *  IXWebSocketServer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocketServer.h"
#include "IXWebSocketTransport.h"
#include "IXWebSocket.h"
#include "IXSocketConnect.h" // for configure, cleanup, move it back to Socket

#include <netdb.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>

namespace ix 
{
    WebSocketServer::WebSocketServer(int port) : 
        _port(port)
    {

    }

    WebSocketServer::~WebSocketServer()
    {

    }

    std::pair<bool, std::string> WebSocketServer::run()
    {
        // https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.hala001/server.htm
        struct sockaddr_in server; /* server address information          */
        int s;                     /* socket for accepting connections    */

        /*
         * Get a socket for accepting connections.
         */
        if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            std::string errMsg = "Socket()";
            return std::make_pair(false, errMsg);
        }

        /*
         * Bind the socket to the server address.
         */
        server.sin_family = AF_INET;
        server.sin_port   = htons(_port);
        server.sin_addr.s_addr = INADDR_ANY;
        // server.sin_addr.s_addr = INADDR_LOOPBACK;

        if (bind(s, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
            std::string errMsg = "Bind()";
            return std::make_pair(false, errMsg);
        }

        /*
         * Listen for connections. Specify the backlog as 1.
         */
        if (listen(s, 1) != 0)
        {
            std::string errMsg = "Listen()";
            return std::make_pair(false, errMsg);
        }

        for (;;)
        {
            /*
             * Accept a connection.
             */
            struct sockaddr_in client; /* client address information          */
            int clientFd; /* socket connected to client */

            socklen_t address_len = sizeof(socklen_t);
            if ((clientFd = accept(s, (struct sockaddr *)&client, &address_len)) == -1)
            {
                std::string errMsg = "Accept()";
                return std::make_pair(false, errMsg);
            }

            _workers.push_back(std::thread(&WebSocketServer::handleConnection, this, clientFd));

            // handleConnection(clientFd);
        }

        return std::make_pair(true, "");
    }

    void WebSocketServer::handleConnection(int fd)
    {
        // We only handle one connection so far, and we just 'print received message from it'
        ix::WebSocketTransport webSocketTransport;
        SocketConnect::configure(fd); // We could/should do this inside initFromSocket
        webSocketTransport.initFromSocket(fd);

        for (;;)
        {
            webSocketTransport.poll();

            // 1. Dispatch the incoming messages
            webSocketTransport.dispatch(
                [&webSocketTransport](const std::string& msg,
                       size_t wireSize,
                       bool decompressionError,
                       WebSocketTransport::MessageKind messageKind)
                {
                    WebSocketMessageType webSocketMessageType;
                    switch (messageKind)
                    {
                        case WebSocketTransport::MSG:
                        {
                            webSocketMessageType = WebSocket_MessageType_Message;
                        } break;

                        case WebSocketTransport::PING:
                        {
                            webSocketMessageType = WebSocket_MessageType_Ping;
                        } break;

                        case WebSocketTransport::PONG:
                        {
                            webSocketMessageType = WebSocket_MessageType_Pong;
                        } break;
                    }

                    WebSocketErrorInfo webSocketErrorInfo;
                    webSocketErrorInfo.decompressionError = decompressionError;

                    // _onMessageCallback(webSocketMessageType, msg, wireSize,
                    //                    webSocketErrorInfo, WebSocketCloseInfo(),
                    //                    WebSocketHttpHeaders());

                    // WebSocket::invokeTrafficTrackerCallback(msg.size(), true);

                    std::cout << "received: " << msg << std::endl;
                    webSocketTransport.sendBinary(msg);
                });

            std::chrono::duration<double, std::milli> wait(10);
            std::this_thread::sleep_for(wait);
        }
    }
}
