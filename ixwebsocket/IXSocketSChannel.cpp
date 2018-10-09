/*
 *  IXSocketSChannel.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 *
 *  See https://docs.microsoft.com/en-us/windows/desktop/WinSock/using-secure-socket-extensions
 */
#include "IXSocketSChannel.h"

#ifdef _WIN32
# include <basetsd.h>
# include <WinSock2.h>
# include <ws2def.h>
# include <WS2tcpip.h>
# include <schannel.h>
# include <sslsock.h>
# include <io.h>

#define WIN32_LEAN_AND_MEAN

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <winsock2.h>
#include <mstcpip.h>
#include <ws2tcpip.h>
#include <rpc.h>
#include <ntdsapi.h>
#include <stdio.h>
#include <tchar.h>

#define RECV_DATA_BUF_SIZE 256

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

// link with fwpuclnt.lib for Winsock secure socket extensions
#pragma comment(lib, "fwpuclnt.lib")

// link with ntdsapi.lib for DsMakeSpn function
#pragma comment(lib, "ntdsapi.lib")

// The following function assumes that Winsock 
// has already been initialized






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
