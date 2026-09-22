
/*
 *  IXSocketFactory.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXProxyOptions.h"
#include "IXSocketTLSOptions.h"
#include <memory>
#include <string>

namespace ix
{
    class Socket;
    std::unique_ptr<Socket> createSocket(bool tls,
                                         int fd,
                                         std::string& errorMsg,
                                         const SocketTLSOptions& tlsOptions,
                                         bool proxy = false,
                                         const ProxyOptions& proxyOptions = {});
} // namespace ix
