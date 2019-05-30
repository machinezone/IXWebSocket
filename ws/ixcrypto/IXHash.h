/*
 *  IXHash.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#pragma once

#include <cstdint>
#include <vector>

namespace ix
{
    uint64_t djb2Hash(const std::vector<uint8_t>& data);
}
