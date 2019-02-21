/*
 *  IXEventFd.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

namespace ix
{
    class EventFd {
    public:
        EventFd();
        virtual ~EventFd();

        bool notify();
        bool clear();
        int getFd();

    private:
        int _eventfd;
    };
}
