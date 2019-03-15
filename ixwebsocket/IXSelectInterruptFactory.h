/*
 *  IXSelectInterruptFactory.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <memory>

namespace ix
{
    class SelectInterrupt;
    std::shared_ptr<SelectInterrupt> createSelectInterrupt();
}
