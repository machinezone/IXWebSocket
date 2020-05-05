/*
 *  IXNetSystem.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#pragma once

#ifdef _WIN32
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <basetsd.h>
#include <io.h>
#include <ws2def.h>

// Define our own poll on Windows, as a wrapper on top of select
typedef unsigned long int nfds_t;

#else
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace ix
{
    bool initNetSystem();
    bool uninitNetSystem();

    int poll(struct pollfd* fds, nfds_t nfds, int timeout);
} // namespace ix
