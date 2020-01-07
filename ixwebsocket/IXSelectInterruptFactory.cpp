/*
 *  IXSelectInterruptFactory.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXSelectInterruptFactory.h"

#if defined(__linux__) || defined(__APPLE__)
#include "IXSelectInterruptPipe.h"
#else
#include "IXSelectInterrupt.h"
#endif

namespace ix
{
    std::shared_ptr<SelectInterrupt> createSelectInterrupt()
    {
#if defined(__linux__) || defined(__APPLE__)
        return std::make_shared<SelectInterruptPipe>();
#else
        return std::make_shared<SelectInterrupt>();
#endif
    }
} // namespace ix
