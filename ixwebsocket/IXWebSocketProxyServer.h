/*
 *  IXWebSocketProxyServer.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019-2020 Machine Zone, Inc. All rights reserved.
 */
#pragma once

#include "IXSocketTLSOptions.h"
#include <cstdint>
#include <stddef.h>
#include <string>

namespace ix
{
    int websocket_proxy_server_main(int port,
                                    const std::string& hostname,
                                    const ix::SocketTLSOptions& tlsOptions,
                                    const std::string& remoteUrl,
                                    bool verbose);
} // namespace ix
