
/*
 *  IXSocketFactory.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <memory>
#include <string>

#include "IXSocketTLSOptions.h"

namespace ix
{
    class Socket;
    std::shared_ptr<Socket> createSocket(bool tls, std::string& errorMsg, const SocketTLSOptions& tlsOptions);

    std::shared_ptr<Socket> createSocket(int fd, std::string& errorMsg);
} // namespace ix
