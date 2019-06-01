/*
 *  IXSocketMbedTLS.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */
#include "IXSocketMbedTLS.h"

namespace ix
{
    SocketMbedTLS::SocketMbedTLS()
    {
        ;
    }

    SocketMbedTLS::~SocketMbedTLS()
    {

    }

    bool SocketMbedTLS::connect(const std::string& host,
                                int port,
                                std::string& errMsg,
                                const CancellationRequest& isCancellationRequested)
    {
        return Socket::connect(host, port, errMsg, nullptr);
    }

    void SocketMbedTLS::close()
    {
        Socket::close();
    }

    ssize_t SocketMbedTLS::send(char* buf, size_t nbyte)
    {
        return Socket::send(buf, nbyte);
    }

    ssize_t SocketMbedTLS::send(const std::string& buffer)
    {
        return Socket::send(buffer);
    }

    ssize_t SocketMbedTLS::recv(void* buf, size_t nbyte)
    {
        return Socket::recv(buf, nbyte);
    }

}
