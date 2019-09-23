/*
 *  IXSocketConnect.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXCancellationRequest.h"
#include "IXSocketTLSOptions.h"
#include <string>

struct addrinfo;

namespace ix
{
    class SocketConnect
    {
    public:
        static int connect(const std::string& hostname,
                           int port,
                           std::string& errMsg,
                           const CancellationRequest& isCancellationRequested,
                           const SocketTLSOptions& tlsOptions = {});

        static void configure(int sockfd);

    private:
        static int connectToAddress(const struct addrinfo* address,
                                    std::string& errMsg,
                                    const CancellationRequest& isCancellationRequested,
                                    const SocketTLSOptions& tlsOptions = {});
    };
} // namespace ix
