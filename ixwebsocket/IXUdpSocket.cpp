/*
 *  IXUdpSocket.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#include "IXUdpSocket.h"

#include "IXNetSystem.h"
#include <cstring>
#include <sstream>

namespace ix
{
    UdpSocket::UdpSocket(int fd)
        : _sockfd(fd)
    {
        ;
    }

    UdpSocket::~UdpSocket()
    {
        close();
    }

    void UdpSocket::close()
    {
        if (_sockfd == -1) return;

        closeSocket(_sockfd);
        _sockfd = -1;
    }

    int UdpSocket::getErrno()
    {
        int err;

#ifdef _WIN32
        err = WSAGetLastError();
#else
        err = errno;
#endif

        return err;
    }

    void UdpSocket::closeSocket(int fd)
    {
#ifdef _WIN32
        closesocket(fd);
#else
        ::close(fd);
#endif
    }

    bool UdpSocket::init(const std::string& host, int port, std::string& errMsg)
    {
        _sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_sockfd < 0)
        {
            errMsg = "Could not create socket";
            return false;
        }

        memset(&_server, 0, sizeof(_server));
        _server.sin_family = AF_INET;
        _server.sin_port = htons(port);

        // DNS resolution.
        struct addrinfo hints, *result = nullptr;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        int ret = getaddrinfo(host.c_str(), nullptr, &hints, &result);
        if (ret != 0)
        {
            errMsg = strerror(UdpSocket::getErrno());
            freeaddrinfo(result);
            close();
            return false;
        }

        struct sockaddr_in* host_addr = (struct sockaddr_in*) result->ai_addr;
        memcpy(&_server.sin_addr, &host_addr->sin_addr, sizeof(struct in_addr));
        freeaddrinfo(result);

        return true;
    }

    ssize_t UdpSocket::sendto(const std::string& buffer)
    {
        return (ssize_t)::sendto(
            _sockfd, buffer.data(), buffer.size(), 0, (struct sockaddr*) &_server, sizeof(_server));
    }
} // namespace ix
