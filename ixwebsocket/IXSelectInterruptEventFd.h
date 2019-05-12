/*
 *  IXSelectInterruptEventFd.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018-2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXSelectInterrupt.h"

#include <stdint.h>
#include <string>

namespace ix
{
    class SelectInterruptEventFd final : public SelectInterrupt {
    public:
        SelectInterruptEventFd();
        virtual ~SelectInterruptEventFd();

        bool init(std::string& errorMsg) final;

        bool notify(uint64_t value) final;
        bool clear() final;
        uint64_t read() final;
        int getFd() const final;

    private:
        int _eventfd;
    };
}

