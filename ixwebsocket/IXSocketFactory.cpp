/*
 *  IXSocketFactory.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXSocketFactory.h"

#ifdef IXWEBSOCKET_USE_TLS

#ifdef IXWEBSOCKET_USE_MBED_TLS
#include <ixwebsocket/IXSocketMbedTLS.h>
#elif __APPLE__
#include <ixwebsocket/IXSocketAppleSSL.h>
#elif defined(_WIN32)
#include <ixwebsocket/IXSocketSChannel.h>
#elif defined(IXWEBSOCKET_USE_OPEN_SSL)
#include <ixwebsocket/IXSocketOpenSSL.h>
#endif

#else

#include <ixwebsocket/IXSocket.h>

#endif

namespace ix
{
    std::shared_ptr<Socket> createSocket(bool tls,
                                         std::string& errorMsg,
                                         const SocketTLSOptions& tlsOptions)
    {
        errorMsg.clear();
        std::shared_ptr<Socket> socket;

        if (!tls)
        {
            socket = std::make_shared<Socket>();
        }
        else
        {
#ifdef IXWEBSOCKET_USE_TLS
#if defined(IXWEBSOCKET_USE_MBED_TLS)
            socket = std::make_shared<SocketMbedTLS>(tlsOptions);
#elif defined(__APPLE__)
            socket = std::make_shared<SocketAppleSSL>(tlsOptions);
#elif defined(_WIN32)
            socket = std::make_shared<SocketSChannel>(tlsOptions);
#else
            socket = std::make_shared<SocketOpenSSL>(tlsOptions);
#endif
#else
            errorMsg = "TLS support is not enabled on this platform.";
            return nullptr;
#endif
        }

        if (!socket->init(errorMsg))
        {
            socket.reset();
        }

        return socket;
    }

    std::shared_ptr<Socket> createSocket(int fd,
                                         bool tls,
                                         std::string& errorMsg,
                                         const SocketTLSOptions& tlsOptions)
    {
        errorMsg.clear();

        std::shared_ptr<Socket> socket;
        if (!tls)
        {
            socket = std::make_shared<Socket>(fd);
        }
        else
        {
#ifdef IXWEBSOCKET_USE_TLS
#if defined(IXWEBSOCKET_USE_MBED_TLS)
            socket = std::make_shared<SocketMbedTLS>(tlsOptions, fd);
#elif defined(__APPLE__)
            socket = std::make_shared<SocketAppleSSL>(tlsOptions, fd);
#elif defined(_WIN32)
            socket = std::make_shared<SocketSChannel>(tlsOptions, fd);
#else
            socket = std::make_shared<SocketOpenSSL>(tlsOptions, fd);
#endif
#else
            errorMsg = "TLS support is not enabled on this platform.";
            return nullptr;
#endif
        }

        if (!socket->init(errorMsg))
        {
            socket.reset();
        }

        return socket;
    }
} // namespace ix