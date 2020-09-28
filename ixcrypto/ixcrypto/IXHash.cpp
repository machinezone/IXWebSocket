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

    uint64_t djb2HashStr(const std::string& data)
    {
        uint64_t hashAddress = 5381;

        for (size_t i = 0; i < data.size(); ++i)
        {
            hashAddress = ((hashAddress << 5) + hashAddress) + data[i];
        }

        return hashAddress;
    }
} // namespace ix
