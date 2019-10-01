
/*
 *  IXSocketFactory.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXSocketTLSOptions.h"
#include <memory>
#include <string>

namespace ix
{
    class Socket;
    std::shared_ptr<Socket> createSocket(bool tls,
                                         int fd,
                                         std::string& errorMsg,
                                         const SocketTLSOptions& tlsOptions);
} // namespace ix
