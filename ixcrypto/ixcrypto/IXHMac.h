/*
 *  IXHMac.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#pragma once

#include <string>

namespace ix
{
    std::string hmac(const std::string& data, const std::string& key);
}
