/*
 *  IXConnectionInfo.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <string>

namespace ix
{
    struct ConnectionInfo
    {
        std::string remoteIp;
        int remotePort;

        ConnectionInfo(const std::string& r = std::string(), int p = 0)
            : remoteIp(r)
            , remotePort(p)
        {
            ;
        }
    };
} // namespace ix
