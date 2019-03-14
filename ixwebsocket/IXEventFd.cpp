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
// On macOS we use UNIX pipes to wake up select.
//

#include "IXEventFd.h"

#ifdef __linux__
# include <sys/eventfd.h>
#endif

#include <unistd.h> // for write
#include <fcntl.h>

namespace ix
{
    // File descriptor at index 0 in _fildes is the read end of the pipe
    // File descriptor at index 1 in _fildes is the write end of the pipe
    const int EventFd::kPipeReadIndex = 0;
    const int EventFd::kPipeWriteIndex = 1;

    EventFd::EventFd()
    {
#ifdef __linux__
        _eventfd = -1;
        _eventfd = eventfd(0, 0);
        fcntl(_eventfd, F_SETFL, O_NONBLOCK);
#else
        _fildes[kPipeReadIndex] = -1;
        _fildes[kPipeWriteIndex] = -1;

        pipe(_fildes);
        fcntl(_fildes[kPipeReadIndex], F_SETFL, O_NONBLOCK);
        fcntl(_fildes[kPipeWriteIndex], F_SETFL, O_NONBLOCK);
#endif
    }

    EventFd::~EventFd()
    {
#ifdef __linux__
        ::close(_eventfd);
#else
        ::close(_fildes[kPipeReadIndex]);
        ::close(_fildes[kPipeWriteIndex]);
        _fildes[kPipeReadIndex] = -1;
        _fildes[kPipeWriteIndex] = -1;
#endif
    }

    bool EventFd::notify(uint64_t value)
    {
        int fd;

#if defined(__linux__)
        fd = _eventfd;
#else
        fd = _fildes[kPipeWriteIndex];
#endif

        if (fd == -1) return false;

        // we should write 8 bytes for an uint64_t
        return write(fd, &value, sizeof(value)) == 8;
    }

    // TODO: return max uint64_t for errors ?
    uint64_t EventFd::read()
    {
        int fd;

#if defined(__linux__)
        fd = _eventfd;
#else
        fd = _fildes[kPipeReadIndex];
#endif
        uint64_t value = 0;
        ::read(fd, &value, sizeof(value));
        return value;
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
#if defined(__linux__)
        return _eventfd;
#else
        return _fildes[kPipeReadIndex];
#endif
    }
}
