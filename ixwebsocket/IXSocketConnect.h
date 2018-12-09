/*
 *  IXSocketConnect.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <string>

struct addrinfo;

namespace ix 
{
    class SocketConnect {
    public:
        static int connect(const std::string& hostname,
                           int port,
                           std::string& errMsg);

        static bool connectToAddress(const struct addrinfo *address, 
                                     int& sockfd,
                                     std::string& errMsg);

        static void configure(int sockfd);
    };
}

