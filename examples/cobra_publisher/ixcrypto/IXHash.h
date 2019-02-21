/*
 *  IXHash.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#pragma once

#include <string>

namespace ix
{
    uint64_t djb2Hash(const std::string& data);
}

