/*
 *  IXProgressCallback.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <functional>

namespace ix
{
    using OnProgressCallback = std::function<bool(int current, int total)>;
}
