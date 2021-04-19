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
        socket_t maxfd = 0;
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
        //
        // It was reported that on Android poll can fail and return -1 with
        // errno == EINTR, which should be a temp error and should typically
        // be handled by retrying in a loop.
        // Maybe we need to put all syscall / C functions in
        // a new IXSysCalls.cpp and wrap them all.
        //
        // The style from libuv is as such.
        //
        int ret = -1;
        do
        {
            ret = ::poll(fds, nfds, timeout);
        } while (ret == -1 && errno == EINTR);

        return ret;
#endif
    }

    //
    // mingw does not have inet_ntop, which were taken as is from the musl C library.
    //
    const char* inet_ntop(int af, const void* a0, char* s, socklen_t l)
    {
#if defined(_WIN32) && defined(__GNUC__)
        const unsigned char* a = (const unsigned char*) a0;
        int i, j, max, best;
        char buf[100];

        switch (af)
        {
            case AF_INET:
                if (snprintf(s, l, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]) < l) return s;
                break;
            case AF_INET6:
                if (memcmp(a, "\0\0\0\0\0\0\0\0\0\0\377\377", 12))
                    snprintf(buf,
                             sizeof buf,
                             "%x:%x:%x:%x:%x:%x:%x:%x",
                             256 * a[0] + a[1],
                             256 * a[2] + a[3],
                             256 * a[4] + a[5],
                             256 * a[6] + a[7],
                             256 * a[8] + a[9],
                             256 * a[10] + a[11],
                             256 * a[12] + a[13],
                             256 * a[14] + a[15]);
                else
                    snprintf(buf,
                             sizeof buf,
                             "%x:%x:%x:%x:%x:%x:%d.%d.%d.%d",
                             256 * a[0] + a[1],
                             256 * a[2] + a[3],
                             256 * a[4] + a[5],
                             256 * a[6] + a[7],
                             256 * a[8] + a[9],
                             256 * a[10] + a[11],
                             a[12],
                             a[13],
                             a[14],
                             a[15]);
                /* Replace longest /(^0|:)[:0]{2,}/ with "::" */
                for (i = best = 0, max = 2; buf[i]; i++)
                {
                    if (i && buf[i] != ':') continue;
                    j = strspn(buf + i, ":0");
                    if (j > max) best = i, max = j;
                }
                if (max > 3)
                {
                    buf[best] = buf[best + 1] = ':';
                    memmove(buf + best + 2, buf + best + max, i - best - max + 1);
                }
                if (strlen(buf) < l)
                {
                    strcpy(s, buf);
                    return s;
                }
                break;
            default: errno = EAFNOSUPPORT; return 0;
        }
        errno = ENOSPC;
        return 0;
#else
        return ::inet_ntop(af, a0, s, l);
#endif
    }
    
#if defined(_WIN32) && defined(__GNUC__)
    static int hexval(unsigned c)
    {
        if (c - '0' < 10) return c - '0';
        c |= 32;
        if (c - 'a' < 6) return c - 'a' + 10;
        return -1;
    }
#endif

    //
    // mingw does not have inet_pton, which were taken as is from the musl C library.
    //
    int inet_pton(int af, const char* s, void* a0)
    {
#if defined(_WIN32) && defined(__GNUC__)
        uint16_t ip[8];
        unsigned char* a = (unsigned char*) a0;
        int i, j, v, d, brk = -1, need_v4 = 0;

        if (af == AF_INET)
        {
            for (i = 0; i < 4; i++)
            {
                for (v = j = 0; j < 3 && isdigit(s[j]); j++)
                    v = 10 * v + s[j] - '0';
                if (j == 0 || (j > 1 && s[0] == '0') || v > 255) return 0;
                a[i] = v;
                if (s[j] == 0 && i == 3) return 1;
                if (s[j] != '.') return 0;
                s += j + 1;
            }
            return 0;
        }
        else if (af != AF_INET6)
        {
            errno = EAFNOSUPPORT;
            return -1;
        }

        if (*s == ':' && *++s != ':') return 0;

        for (i = 0;; i++)
        {
            if (s[0] == ':' && brk < 0)
            {
                brk = i;
                ip[i & 7] = 0;
                if (!*++s) break;
                if (i == 7) return 0;
                continue;
            }
            for (v = j = 0; j < 4 && (d = hexval(s[j])) >= 0; j++)
                v = 16 * v + d;
            if (j == 0) return 0;
            ip[i & 7] = v;
            if (!s[j] && (brk >= 0 || i == 7)) break;
            if (i == 7) return 0;
            if (s[j] != ':')
            {
                if (s[j] != '.' || (i < 6 && brk < 0)) return 0;
                need_v4 = 1;
                i++;
                break;
            }
            s += j + 1;
        }
        if (brk >= 0)
        {
            memmove(ip + brk + 7 - i, ip + brk, 2 * (i + 1 - brk));
            for (j = 0; j < 7 - i; j++)
                ip[brk + j] = 0;
        }
        for (j = 0; j < 8; j++)
        {
            *a++ = ip[j] >> 8;
            *a++ = ip[j];
        }
        if (need_v4 && inet_pton(AF_INET, (const char*) s, a - 4) <= 0) return 0;
        return 1;
#else
        return ::inet_pton(af, s, a0);
#endif
    }

} // namespace ix
