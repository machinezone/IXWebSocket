/*
 *  IXEventFd.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

//
// Linux/Android has a special type of virtual files. select(2) will react
// when reading/writing to those files, unlike closing sockets.
//
// https://linux.die.net/man/2/eventfd
// http://www.sourcexr.com/articles/2013/10/26/lightweight-inter-process-signaling-with-eventfd
//
// eventfd was added in Linux kernel 2.x, and our oldest Android (Kitkat 4.4)
// is on Kernel 3.x
//
// cf Android/Kernel table here
// https://android.stackexchange.com/questions/51651/which-android-runs-which-linux-kernel
//

#include "IXEventFd.h"

#ifdef __linux__
# include <sys/eventfd.h>
#endif

#ifndef _WIN32
#include <unistd.h> // for write
#endif

namespace ix
{
    EventFd::EventFd() :
        _eventfd(-1)
    {
#ifdef __linux__
        _eventfd = eventfd(0, 0);
#endif
    }

    EventFd::~EventFd()
    {
#ifdef __linux__
        ::close(_eventfd);
#endif
    }

    bool EventFd::notify()
    {
#if defined(__linux__)
        if (_eventfd == -1) return false;

        // select will wake up when a non-zero value is written to our eventfd
        uint64_t value = 1;

        // we should write 8 bytes for an uint64_t
        return write(_eventfd, &value, sizeof(value)) == 8;
#else
        return true;
#endif
    }

    bool EventFd::clear()
    {
#if defined(__linux__)
        if (_eventfd == -1) return false;

        // 0 is a special value ; select will not wake up
        uint64_t value = 0;

        // we should write 8 bytes for an uint64_t
        return write(_eventfd, &value, sizeof(value)) == 8;
#else
        return true;
#endif
    }

    int EventFd::getFd()
    {
        return _eventfd;
    }
}
