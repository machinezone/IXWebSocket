/*
 *  IXSocket.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXSocket.h"

#ifdef _WIN32
# include <basetsd.h>
# include <WinSock2.h>
# include <ws2def.h>
# include <WS2tcpip.h>
# include <io.h>
#else
# include <unistd.h>
# include <errno.h>
# include <netdb.h>
# include <netinet/tcp.h>
# include <sys/socket.h>
# include <sys/time.h>
# include <sys/select.h>
# include <sys/stat.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>

#include <algorithm>
#include <iostream>

// Android needs extra headers for TCP_NODELAY and IPPROTO_TCP
#ifdef ANDROID
# include <linux/in.h>
# include <linux/tcp.h>
#endif

namespace ix 
{
    Socket::Socket() : 
        _sockfd(-1)
    {

    }

    Socket::~Socket()
    {
        close();
    }

    bool Socket::connectToAddress(const struct addrinfo *address, 
                                  int& sockfd,
                                  std::string& errMsg)
    {
        sockfd = -1;

        int fd = socket(address->ai_family,
                        address->ai_socktype,
                        address->ai_protocol);
        if (fd < 0)
        {
            errMsg = "Cannot create a socket";
            return false;
        }

        int maxRetries = 3;
        for (int i = 0; i < maxRetries; ++i)
        {
            if (::connect(fd, address->ai_addr, address->ai_addrlen) != -1)
            {
                sockfd = fd;
                return true;
            }

            // EINTR means we've been interrupted, in which case we try again.
            if (errno != EINTR) break;
        }

        closeSocket(fd);
        sockfd = -1;
        errMsg = strerror(errno);
        return false;
    }

    int Socket::hostname_connect(const std::string& hostname,
                                 int port,
                                 std::string& errMsg)
    {
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        std::string sport = std::to_string(port);

        struct addrinfo *res = nullptr;
        int getaddrinfo_result = getaddrinfo(hostname.c_str(), sport.c_str(), 
                                             &hints, &res);
        if (getaddrinfo_result)
        {
            errMsg = gai_strerror(getaddrinfo_result);
            return -1;
        }

        int sockfd = -1;

        // iterate through the records to find a working peer
        struct addrinfo *address;
        bool success = false;
        for (address = res; address != nullptr; address = address->ai_next)
        {
            success = connectToAddress(address, sockfd, errMsg);
            if (success)
            {
                break;
            }
        }
        freeaddrinfo(res);
        return sockfd;
    }

    void Socket::configure()
    {
        int flag = 1;
        setsockopt(_sockfd, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(flag)); // Disable Nagle's algorithm

#ifdef _WIN32
        unsigned long nonblocking = 1;
        ioctlsocket(_sockfd, FIONBIO, &nonblocking);
#else
        fcntl(_sockfd, F_SETFL, O_NONBLOCK); // make socket non blocking
#endif

#ifdef SO_NOSIGPIPE
        int value = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_NOSIGPIPE, 
                   (void *)&value, sizeof(value));
#endif
    }

    void Socket::poll(const OnPollCallback& onPollCallback)
    {
        if (_sockfd == -1)
        {
            onPollCallback();
            return;
        }

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(_sockfd, &rfds);

#ifdef __linux__
        FD_SET(_eventfd.getFd(), &rfds);
#endif

        int sockfd = _sockfd;
        int nfds = (std::max)(sockfd, _eventfd.getFd());
        select(nfds + 1, &rfds, nullptr, nullptr, nullptr);

        onPollCallback();
    }

    void Socket::wakeUpFromPoll()
    {
        // this will wake up the thread blocked on select, only needed on Linux
        _eventfd.notify();
    }

    bool Socket::connect(const std::string& host,
                         int port,
                         std::string& errMsg)
    {
        std::lock_guard<std::mutex> lock(_socketMutex);

        if (!_eventfd.clear()) return false;

        _sockfd = Socket::hostname_connect(host, port, errMsg);
        return _sockfd != -1;
    }

    void Socket::close()
    {
        std::lock_guard<std::mutex> lock(_socketMutex);

        if (_sockfd == -1) return;

        closeSocket(_sockfd);
        _sockfd = -1;
    }

    int Socket::send(char* buffer, size_t length)
    {
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags = MSG_NOSIGNAL;
#endif

        return (int) ::send(_sockfd, buffer, length, flags);
    }

    int Socket::send(const std::string& buffer)
    {
        return send((char*)&buffer[0], buffer.size());
    }

    int Socket::recv(void* buffer, size_t length)
    {
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags = MSG_NOSIGNAL;
#endif

        return (int) ::recv(_sockfd, (char*) buffer, length, flags);
    }

    int Socket::getErrno() const
    {
#ifdef _WIN32
        return WSAGetLastError();
#else
        return errno;
#endif
    }

    void Socket::closeSocket(int fd)
    {
#ifdef _WIN32
        closesocket(fd);
#else
        ::close(fd);
#endif
    }

    bool Socket::init()
    {
#ifdef _WIN32
        INT rc;
        WSADATA wsaData;
        
        rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
        return rc != 0;
#else
        return true;
#endif
    }

    void Socket::cleanup()
    {
#ifdef _WIN32
        WSACleanup();
#endif
    }
}
