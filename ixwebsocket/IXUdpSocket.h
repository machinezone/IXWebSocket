/*
 *  IXUdpSocket.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>

#include "IXNetSystem.h"

namespace ix
{
    class UdpSocket
    {
    public:
        UdpSocket(int fd = -1);
        ~UdpSocket();

        // Virtual methods
        bool init(const std::string& host, int port, std::string& errMsg);
        std::ptrdiff_t sendto(const std::string& buffer);
        std::ptrdiff_t recvfrom(char* buffer, size_t length);

        void close();

        static int getErrno();
        static bool isWaitNeeded();
        static void closeSocket(socket_t fd);

    private:
        std::atomic<int> _sockfd;
        struct sockaddr_in _server;
    };
} // namespace ix
