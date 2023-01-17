/*
 *  IXSocketConnect.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXCancellationRequest.h"
#ifndef proxysocteksettings_h
    #include "proxysocteksetting.h"
#endif

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
                           const CancellationRequest& isCancellationRequested, const ProxySetup &proxy_settings);

        static void configure(int sockfd);


    private:
        static int connectToAddressViaProxy(const std::string& host,
                                            int port,
                                            std::string& errMsg, const ProxySetup &proxy_settings);

        static int connectToAddress(const struct addrinfo* address,
                                    std::string& errMsg,
                                    const CancellationRequest& isCancellationRequested);
    };
} // namespace ix
