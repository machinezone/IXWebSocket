/*
 *  IXUdpSocket.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;

#undef EWOULDBLOCK
#undef EAGAIN
#undef EINPROGRESS
#undef EBADF
#undef EINVAL

// map to WSA error codes
#define EWOULDBLOCK WSAEWOULDBLOCK
#define EAGAIN WSATRY_AGAIN
#define EINPROGRESS WSAEINPROGRESS
#define EBADF WSAEBADF
#define EINVAL WSAEINVAL

#endif

#include "IXCancellationRequest.h"
#include "IXNetSystem.h"

namespace ix
{
    class SelectInterrupt;

    class UdpSocket
    {
    public:
        UdpSocket(int fd = -1);
        virtual ~UdpSocket();

        // Virtual methods
        bool init(const std::string& host, int port, std::string& errMsg);
        ssize_t sendto(const std::string& buffer);
        virtual void close();

        static int getErrno();
        static bool isWaitNeeded();
        static void closeSocket(int fd);

    protected:
        std::atomic<int> _sockfd;
        std::mutex _socketMutex;

        struct sockaddr_in _server;

    private:
        std::shared_ptr<SelectInterrupt> _selectInterrupt;
    };
} // namespace ix
