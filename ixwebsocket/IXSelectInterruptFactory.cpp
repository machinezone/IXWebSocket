/*
 *  IXSelectInterruptFactory.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXSelectInterruptFactory.h"

#if defined(__linux__)
# include <ixwebsocket/IXSelectInterruptEventFd.h>
#else
# include <ixwebsocket/IXSelectInterruptPipe.h>
#endif

namespace ix
{
    std::shared_ptr<SelectInterrupt> createSelectInterrupt()
    {
#if defined(__linux__)
        return std::make_shared<SelectInterruptEventFd>();
#else
        return std::make_shared<SelectInterruptPipe>();
#endif
    }
}
