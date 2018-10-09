/*
 *  IXSocketSChannel.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */
#include "IXSocketSChannel.h"

#ifdef _WIN32
# include <basetsd.h>
# include <WinSock2.h>
# include <ws2def.h>
# include <WS2tcpip.h>
# include <schannel.h>
# include <io.h>
#else
# error("This file should only be built on Windows")
#endif

namespace ix 
{
    SocketSChannel::SocketSChannel()
    {
        ;
    }

    SocketSChannel::~SocketSChannel()
    {
        
    }

    bool SocketSChannel::connect(const std::string& host,
                                 int port,
                                 std::string& errMsg)
    {
        return Socket::connect(host, port, errMsg);
    }

    
    void SocketSChannel::secureSocket()
    {
        DWORD dwOptVal = SO_SEC_SSL;
        DWORD dwBytes = 0;
        SSLVALIDATECERTHOOK sslValidateFunc;

        sockerror = setsockopt(_sockfd, SOL_SOCKET,
                               SO_SECURE, (LPSTR)&dwOptVal, sizeof(dwOptVal));

        // FIXME do something with sockerror
    }

    void SocketSChannel::close()
    {
        Socket::close();
    }

    int SocketSChannel::send(char* buf, size_t nbyte)
    {
        return Socket::send(buf, nbyte);
    }

    int SocketSChannel::send(const std::string& buffer)
    {
        return Socket::send(buffer);
    }

    int SocketSChannel::recv(void* buf, size_t nbyte)
    {
        return Socket::recv(buf, nbyte);
    }

}
