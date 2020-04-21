/*
 *  IXHash.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#include "IXHash.h"

namespace ix
{
    uint64_t djb2Hash(const std::vector<uint8_t>& data)
    {
        uint64_t hashAddress = 5381;

        for (auto&& c : data)
        {
            hashAddress = ((hashAddress << 5) + hashAddress) + c;
        }

        return hashAddress;
    }
} // namespace ix
