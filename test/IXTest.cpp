/*
 *  IXTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXNetSystem.h>

#include <chrono>
#include <thread>
#include <mutex>
#include <string>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <stack>

namespace ix
{
    std::atomic<size_t> incomingBytes(0);
    std::atomic<size_t> outgoingBytes(0);
    std::mutex Logger::_mutex;
    std::stack<int> freePorts;

    void setupWebSocketTrafficTrackerCallback()
    {
        ix::WebSocket::setTrafficTrackerCallback(
            [](size_t size, bool incoming)
            {
                if (incoming)
                {
                    incomingBytes += size;
                }
                else
                {
                    outgoingBytes += size;
                }
            }
        );
    }

    void reportWebSocketTraffic()
    {
        Logger() << incomingBytes;
        Logger() << "Incoming bytes: " << incomingBytes;
        Logger() << "Outgoing bytes: " << outgoingBytes;
    }

    void msleep(int ms)
    {
        std::chrono::duration<double, std::milli> duration(ms);
        std::this_thread::sleep_for(duration);
    }

    std::string generateSessionId()
    {
        auto now = std::chrono::system_clock::now();
        auto seconds = 
            std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch()).count();

        return std::to_string(seconds);
    }

    void log(const std::string& msg)
    {
        Logger() << msg;
    }

    int getFreePort()
    {
        int defaultPort = 8090;

        int sockfd; 
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            log("Cannot compute a free port. socket error.");
            return defaultPort;
        }

        int enable = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                       (char*) &enable, sizeof(enable)) < 0)
        {
            log("Cannot compute a free port. setsockopt error.");
            return defaultPort;
        }

        // Bind to port 0. This is the standard way to get a free port.
        struct sockaddr_in server; // server address information
        server.sin_family = AF_INET;
        server.sin_port   = htons(0);
        server.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
            log("Cannot compute a free port. bind error.");

            ::close(sockfd);
            return defaultPort;
        }

        struct sockaddr_in sa; // server address information
        unsigned int len;
        if (getsockname(sockfd, (struct sockaddr *) &sa, &len) < 0)
        {
            log("Cannot compute a free port. getsockname error.");

            ::close(sockfd);
            return defaultPort;
        }

        int port = ntohs(sa.sin_port);
        ::close(sockfd);

        return port;
    }
}
