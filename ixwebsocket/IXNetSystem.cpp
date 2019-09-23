/*
 *  IXNetSystem.cpp
 *  Author: Korchynskyi Dmytro
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#include "IXNetSystem.h"

namespace ix
{
    bool initNetSystem()
    {
#ifdef _WIN32
        WORD wVersionRequested;
        WSADATA wsaData;
        int err;

        // Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h
        wVersionRequested = MAKEWORD(2, 2);
        err = WSAStartup(wVersionRequested, &wsaData);

        return err == 0;
#else
        return true;
#endif
    }

    bool uninitNetSystem()
    {
#ifdef _WIN32
        int err = WSACleanup();
        return err == 0;
#else
        return true;
#endif
    }

    //
    // That function could 'return WSAPoll(pfd, nfds, timeout);'
    // but WSAPoll is said to have weird behaviors on the internet
    // (the curl folks have had problems with it).
    //
    // So we make it a select wrapper
    //
    int poll(struct pollfd* fds, nfds_t nfds, int timeout)
    {
#ifdef _WIN32
        int maxfd = 0;
        fd_set readfds, writefds, errorfds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&errorfds);

        for (nfds_t i = 0; i < nfds; ++i)
        {
            struct pollfd* fd = &fds[i];

            if (fd->fd > maxfd)
            {
                maxfd = fd->fd;
            }
            if ((fd->events & POLLIN))
            {
                FD_SET(fd->fd, &readfds);
            }
            if ((fd->events & POLLOUT))
            {
                FD_SET(fd->fd, &writefds);
            }
            if ((fd->events & POLLERR))
            {
                FD_SET(fd->fd, &errorfds);
            }
        }

        struct timeval tv;
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;

        int ret = select(maxfd + 1, &readfds, &writefds, &errorfds, timeout != -1 ? &tv : NULL);

        if (ret < 0)
        {
            return ret;
        }

        for (nfds_t i = 0; i < nfds; ++i)
        {
            struct pollfd* fd = &fds[i];
            fd->revents = 0;

            if (FD_ISSET(fd->fd, &readfds))
            {
                fd->revents |= POLLIN;
            }
            if (FD_ISSET(fd->fd, &writefds))
            {
                fd->revents |= POLLOUT;
            }
            if (FD_ISSET(fd->fd, &errorfds))
            {
                fd->revents |= POLLERR;
            }
        }

        return ret;
#else
        return ::poll(fds, nfds, timeout);
#endif
    }

} // namespace ix
