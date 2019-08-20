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

static inline int poll(struct pollfd *pfd, unsigned long nfds, int timeout)
{
    return WSAPoll(pfd, nfds, timeout);
}

#else
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <poll.h>
#endif

namespace ix
{
    bool initNetSystem();
    bool uninitNetSystem();
} // namespace ix
