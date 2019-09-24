/*
 *  base64.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#pragma once

#include <string>

namespace ix
{
    std::string base64_encode(const std::string& data, size_t len);
    std::string base64_encode(const char* data, size_t len);
    std::string base64_decode(const std::string& encoded_string);
} // namespace ix
