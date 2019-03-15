/*
 *  IXSelectInterruptEventFd.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018-2019 Machine Zone, Inc. All rights reserved.
 */

//
// On Linux we use eventd to wake up select.
//

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

#include "IXSelectInterruptEventFd.h"

#include <sys/eventfd.h>

#include <unistd.h> // for write
#include <string.h> // for strerror
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sstream>

namespace ix
{
    SelectInterruptEventFd::SelectInterruptEventFd()
    {
        _eventfd = -1;
    }

    SelectInterruptEventFd::~SelectInterruptEventFd()
    {
        ::close(_eventfd);
    }

    bool SelectInterruptEventFd::init(std::string& errorMsg)
    {
        // calling init twice is a programming error
        assert(_eventfd == -1);

        _eventfd = eventfd(0, 0);
        if (_eventfd < 0)
        {
            std::stringstream ss;
            ss << "SelectInterruptEventFd::init() failed in eventfd()"
               << " : " << strerror(errno);
            errorMsg = ss.str();

            _eventfd = -1;
            return false;
        }

        if (fcntl(_eventfd, F_SETFL, O_NONBLOCK) == -1)
        {
            std::stringstream ss;
            ss << "SelectInterruptEventFd::init() failed in fcntl() call"
               << " : " << strerror(errno);
            errorMsg = ss.str();

            _eventfd = -1;
            return false;
        }

        return true;
    }

    bool SelectInterruptEventFd::notify(uint64_t value)
    {
        int fd = _eventfd;

        if (fd == -1) return false;

        // we should write 8 bytes for an uint64_t
        return write(fd, &value, sizeof(value)) == 8;
    }

    // TODO: return max uint64_t for errors ?
    uint64_t SelectInterruptEventFd::read()
    {
        int fd = _eventfd;

        uint64_t value = 0;
        ::read(fd, &value, sizeof(value));
        return value;
    }

    bool SelectInterruptEventFd::clear()
    {
        if (_eventfd == -1) return false;

        // 0 is a special value ; select will not wake up
        uint64_t value = 0;

        // we should write 8 bytes for an uint64_t
        return write(_eventfd, &value, sizeof(value)) == 8;
    }

    int SelectInterruptEventFd::getFd() const
    {
        return _eventfd;
    }
}
