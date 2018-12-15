/*
 *  IXSocketConnect.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXCancellationRequest.h"

#include <string>

struct addrinfo;

namespace ix 
{
    class SocketConnect {
    public:
        static int connect(const std::string& hostname,
                           int port,
                           std::string& errMsg,
                           const CancellationRequest& isCancellationRequested);

    private:
        static bool connectToAddress(const struct addrinfo *address, 
                                     int& sockfd,
                                     std::string& errMsg,
                                     const CancellationRequest& isCancellationRequested);

        static void configure(int sockfd);
    };
}

