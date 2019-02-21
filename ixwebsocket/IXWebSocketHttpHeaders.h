/*
 *  IXWebSocketHttpHeaders.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <string>
#include <unordered_map>

namespace ix
{
    using WebSocketHttpHeaders = std::unordered_map<std::string, std::string>;
}
