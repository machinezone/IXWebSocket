/*
 *  IXSocketConnect.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <string>
#include <functional>

struct addrinfo;

namespace ix 
{
    using CancellationRequest = std::function<bool()>;

    class SocketConnect {
    public:
        static int connect(const std::string& hostname,
                           int port,
                           std::string& errMsg,
                           CancellationRequest isCancellationRequested);

        static bool connectToAddress(const struct addrinfo *address, 
                                     int& sockfd,
                                     std::string& errMsg,
                                     CancellationRequest isCancellationRequested);

        static void configure(int sockfd);
    };
}

