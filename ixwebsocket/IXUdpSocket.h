/*
 *  IXUdpSocket.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <functional>
#include <vector>

#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#include "IXNetSystem.h"

namespace ix
{
    using OnDataReveive = std::function<void(const std::vector<uint8_t>&)>;

    class UdpSocket
    {
    public:
        UdpSocket(int fd = -1);
        ~UdpSocket();

        // Virtual methods
        bool init(const std::string& host, int port, std::string& errMsg);
        ssize_t sendto(const std::string& buffer);
        void receive_async(const OnDataReveive &receiver, size_t Size);
        void close();

        static int getErrno();
        static void closeSocket(int fd);

    private:
        std::atomic<int> _sockfd;
        struct sockaddr_in _server;
    };
} // namespace ix
