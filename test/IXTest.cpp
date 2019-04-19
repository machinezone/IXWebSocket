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
#include <iomanip>
#include <random>


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

    int getAnyFreePortRandom()
    {
        std::random_device rd;
        std::uniform_int_distribution<int> dist(1024 + 1, 65535);

        return dist(rd);
    }

    int getAnyFreePort()
    {
        int defaultPort = 8090;
        int sockfd;
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            log("Cannot compute a free port. socket error.");
            return getAnyFreePortRandom();
        }

        int enable = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                       (char*) &enable, sizeof(enable)) < 0)
        {
            log("Cannot compute a free port. setsockopt error.");
            return getAnyFreePortRandom();
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
            return getAnyFreePortRandom();
        }

        struct sockaddr_in sa; // server address information
        socklen_t len;
        if (getsockname(sockfd, (struct sockaddr *) &sa, &len) < 0)
        {
            log("Cannot compute a free port. getsockname error.");

            ::close(sockfd);
            return getAnyFreePortRandom();
        }

        int port = ntohs(sa.sin_port);
        ::close(sockfd);

        return port;
    }

    int getFreePort()
    {
        while (true)
        {
#if defined(__has_feature)
# if __has_feature(address_sanitizer)
            int port = getAnyFreePortRandom();
# else
            int port = getAnyFreePort();
# endif
#else
            int port = getAnyFreePort();
#endif
            //
            // Only port above 1024 can be used by non root users, but for some
            // reason I got port 7 returned with macOS when binding on port 0...
            //
            if (port > 1024)
            {
                return port;
            }
        }

        return -1;
    }

    void hexDump(const std::string& prefix,
                 const std::string& s)
    {
        std::ostringstream ss;
        bool upper_case = false;

        for (std::string::size_type i = 0; i < s.length(); ++i)
        {
            ss << std::hex
               << std::setfill('0')
               << std::setw(2)
               << (upper_case ? std::uppercase : std::nouppercase) << (int)s[i];
        }

        std::cout << prefix << ": " << s << " => " << ss.str() << std::endl;
    }
}
