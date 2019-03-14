/*
 *  IXEventFd.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <stdint.h>

namespace ix
{
    class EventFd {
    public:
        EventFd();
        virtual ~EventFd();

        bool notify(uint64_t value);
        bool clear();
        uint64_t read();
        int getFd();

    private:
#if defined(__linux__)
        int _eventfd;
#else
        // Store file descriptors used by the communication pipe. Communication
        // happens between a control thread and a background thread, which is
        // blocked on select.
        int _fildes[2];
#endif
    };
}
