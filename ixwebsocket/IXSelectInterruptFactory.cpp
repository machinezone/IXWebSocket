/*
 *  IXSelectInterruptFactory.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXSelectInterruptFactory.h"

#include "IXUniquePtr.h"
#if defined(__linux__) || defined(__APPLE__)
#include "IXSelectInterruptPipe.h"
#else
#include "IXSelectInterrupt.h"
#endif

namespace ix
{
    SelectInterruptPtr createSelectInterrupt()
    {
#if defined(__linux__) || defined(__APPLE__)
        return ix::make_unique<SelectInterruptPipe>();
#else
        return ix::make_unique<SelectInterrupt>();
#endif
    }
} // namespace ix
