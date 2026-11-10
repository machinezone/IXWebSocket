/*
 *  IXSocks5.h
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#pragma once

#include "IXCancellationRequest.h"
#include "IXNetSystem.h"
#include "IXProxyOptions.h"
#include <string>

namespace ix
{
    class Socks5
    {
    public:
        static bool validate(const ProxyOptions& options, std::string& error);
        static std::string resolveHost(const std::string& host,
                                       int port,
                                       std::string& error,
                                       const CancellationRequest& cancelled);
        static bool connect(socket_t fd,
                            const std::string& host,
                            int port,
                            const ProxyOptions& options,
                            std::string& error,
                            const CancellationRequest& cancelled);
    };
} // namespace ix
