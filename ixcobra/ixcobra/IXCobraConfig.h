/*
 *  IXCobraConfig.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <ixwebsocket/IXSocketTLSOptions.h>
#include <ixwebsocket/IXWebSocketPerMessageDeflateOptions.h>

namespace ix
{
    struct CobraConfig
    {
        std::string appkey;
        std::string endpoint;
        std::string rolename;
        std::string rolesecret;
        WebSocketPerMessageDeflateOptions webSocketPerMessageDeflateOptions;
        SocketTLSOptions socketTLSOptions;

        CobraConfig(const std::string& a = std::string(),
                    const std::string& e = std::string(),
                    const std::string& r = std::string(),
                    const std::string& s = std::string())
            : appkey(a)
            , endpoint(e)
            , rolename(r)
            , rolesecret(s)
        {
            ;
        }
    };
} // namespace ix
