/*
 *  IXSocket.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXSocket.h"
#include "IXSocketConnect.h"

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

namespace ix 
{
    Socket::Socket(int fd) : 
        _sockfd(fd)
    {

    }

    Socket::~Socket()
    {
        close();
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
                         std::string& errMsg,
                         const CancellationRequest& isCancellationRequested)
    {
        std::lock_guard<std::mutex> lock(_socketMutex);

        if (!_eventfd.clear()) return false;

        _sockfd = SocketConnect::connect(host, port, errMsg, isCancellationRequested);
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

    int Socket::getErrno()
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

    bool Socket::readByte(void* buffer,
                          const CancellationRequest& isCancellationRequested)
    {
        while (true)
        {
            if (isCancellationRequested()) return false;

            int ret;
            ret = recv(buffer, 1);

            // We read one byte, as needed, all good.
            if (ret == 1)
            {
                return true;
            }
            // There is possibly something to be read, try again
            else if (ret < 0 && (getErrno() == EWOULDBLOCK ||
                                 getErrno() == EAGAIN))
            {
                // Wait with a timeout until something is written.
                // This way we are not busy looping
                fd_set rfds;
                struct timeval timeout;
                timeout.tv_sec = 0;
                timeout.tv_usec = 1 * 1000; // 1ms

                FD_ZERO(&rfds);
                FD_SET(_sockfd, &rfds);
                select(_sockfd + 1, &rfds, nullptr, nullptr, &timeout);

                continue;
            }
            // There was an error during the read, abort
            else
            {
                return false;
            }
        }
    }

    bool Socket::writeBytes(const std::string& str,
                            const CancellationRequest& isCancellationRequested)
    {
        while (true)
        {
            if (isCancellationRequested()) return false;

            char* buffer = const_cast<char*>(str.c_str());
            int len = (int) str.size();

            int ret = send(buffer, len);

            // We wrote some bytes, as needed, all good.
            if (ret > 0)
            {
                return ret == len;
            }
            // There is possibly something to be write, try again
            else if (ret < 0 && (getErrno() == EWOULDBLOCK ||
                                 getErrno() == EAGAIN))
            {
                continue;
            }
            // There was an error during the write, abort
            else
            {
                return false;
            }
        }
    }

    std::pair<bool, std::string> Socket::readLine(const CancellationRequest& isCancellationRequested)
    {
        char c;
        std::string line;
        line.reserve(64);

        for (int i = 0; i < 2 || (line[i-2] != '\r' && line[i-1] != '\n'); ++i)
        {
            if (!readByte(&c, isCancellationRequested))
            {
                return std::make_pair(false, std::string());
            }

            line += c;
        }

        return std::make_pair(true, line);
    }
}
