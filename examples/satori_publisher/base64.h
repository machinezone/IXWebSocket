/*
 *  base64.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#pragma once

#include <string>

namespace ix
{
    std::string base64_encode(const std::string& data, uint32_t len);
}
