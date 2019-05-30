/*
 *  IXSelectInterrupt.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <stdint.h>
#include <string>

namespace ix
{
    class SelectInterrupt
    {
    public:
        SelectInterrupt();
        virtual ~SelectInterrupt();

        virtual bool init(std::string& errorMsg);

        virtual bool notify(uint64_t value);
        virtual bool clear();
        virtual uint64_t read();
        virtual int getFd() const;
    };
} // namespace ix
