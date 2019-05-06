/*
 *  IXSocketConnect.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXSocketConnect.h"
#include "IXDNSLookup.h"
#include "IXNetSystem.h"
#include "IXSocket.h"

#include <string.h>
#include <fcntl.h>
#include <sys/types.h>

// Android needs extra headers for TCP_NODELAY and IPPROTO_TCP
#ifdef ANDROID
# include <linux/in.h>
# include <linux/tcp.h>
#endif

namespace ix
{
    //
    // This function can be cancelled every 50 ms
    // This is important so that we don't block the main UI thread when shutting down a connection which is
    // already trying to reconnect, and can be blocked waiting for ::connect to respond.
    //
    int SocketConnect::connectToAddress(const struct addrinfo *address,
                                        std::string& errMsg,
                                        const CancellationRequest& isCancellationRequested)
    {
        errMsg = "no error";

        int fd = socket(address->ai_family,
                        address->ai_socktype,
                        address->ai_protocol);
        if (fd < 0)
        {
            errMsg = "Cannot create a socket";
            return -1;
        }

        // Set the socket to non blocking mode, so that slow responses cannot
        // block us for too long
        SocketConnect::configure(fd);

        int res = ::connect(fd, address->ai_addr, address->ai_addrlen);

        if (res == -1 && !Socket::isWaitNeeded())
        {
            errMsg = strerror(Socket::getErrno());
            Socket::closeSocket(fd);
            return -1;
        }

        for (;;)
        {
            if (isCancellationRequested && isCancellationRequested()) // Must handle timeout as well
            {
                Socket::closeSocket(fd);
                errMsg = "Cancelled";
                return -1;
            }

            // On Linux the timeout needs to be re-initialized everytime
            // http://man7.org/linux/man-pages/man2/select.2.html
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 10 * 1000; // 10ms timeout

            fd_set wfds;
            fd_set efds;

            FD_ZERO(&wfds);
            FD_SET(fd, &wfds);
            FD_ZERO(&efds);
            FD_SET(fd, &efds);

            // Use select to check the status of the new connection
            res = select(fd + 1, nullptr, &wfds, &efds, &timeout);

            if (res < 0 && (Socket::getErrno() == EBADF || Socket::getErrno() == EINVAL))
            {
                Socket::closeSocket(fd);
                errMsg = std::string("Connect error, select error: ") + strerror(Socket::getErrno());
                return -1;
            }

            // Nothing was written to the socket, wait again.
            if (!FD_ISSET(fd, &wfds)) continue;

            // Something was written to the socket. Check for errors.
            int optval = -1;
            socklen_t optlen = sizeof(optval);

#ifdef _WIN32
            // On connect error, in async mode, windows will write to the exceptions fds
            if (FD_ISSET(fd, &efds))
#else
            // getsockopt() puts the errno value for connect into optval so 0
            // means no-error.
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1 ||
                optval != 0)
#endif
            {
                Socket::closeSocket(fd);
                errMsg = strerror(optval);
                return -1;
            }
            else
            {
                // Success !
                return fd;
            }
        }

        Socket::closeSocket(fd);
        errMsg = "connect timed out after 60 seconds";
        return -1;
    }

    int SocketConnect::connect(const std::string& hostname,
                               int port,
                               std::string& errMsg,
                               const CancellationRequest& isCancellationRequested)
    {
        //
        // First do DNS resolution
        //
        DNSLookup dnsLookup(hostname, port);
        struct addrinfo *res = dnsLookup.resolve(errMsg, isCancellationRequested);
        if (res == nullptr)
        {
            return -1;
        }

        int sockfd = -1;

        // iterate through the records to find a working peer
        struct addrinfo *address;
        for (address = res; address != nullptr; address = address->ai_next)
        {
            //
            // Second try to connect to the remote host
            //
            sockfd = connectToAddress(address, errMsg, isCancellationRequested);
            if (sockfd != -1)
            {
                break;
            }
        }

        freeaddrinfo(res);
        return sockfd;
    }

    // FIXME: configure is a terrible name
    void SocketConnect::configure(int sockfd)
    {
        // 1. disable Nagle's algorithm
        int flag = 1;
        setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(flag));

        // 2. make socket non blocking
#ifdef _WIN32
        unsigned long nonblocking = 1;
        ioctlsocket(sockfd, FIONBIO, &nonblocking);
#else
        fcntl(sockfd, F_SETFL, O_NONBLOCK); // make socket non blocking
#endif

        // 3. (apple) prevent SIGPIPE from being emitted when the remote end disconnect
#ifdef SO_NOSIGPIPE
        int value = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE,
                   (void *)&value, sizeof(value));
#endif
    }
}
