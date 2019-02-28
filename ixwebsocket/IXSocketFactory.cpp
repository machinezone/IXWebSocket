/*
 *  IXSocketFactory.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXSocketFactory.h"

#if defined(__APPLE__) or defined(__linux__)
# ifdef __APPLE__
#  include <ixwebsocket/IXSocketAppleSSL.h>
# else
#  include <ixwebsocket/IXSocketOpenSSL.h>
# endif
#endif

namespace ix
{
    std::shared_ptr<Socket> createSocket(bool tls,
                                         std::string& errorMsg)
    {
        errorMsg.clear();

        if (!tls)
        {
            return std::make_shared<Socket>();
        }
        else
        {
#ifdef IXWEBSOCKET_USE_TLS
# ifdef __APPLE__
            return std::make_shared<SocketAppleSSL>();
# else
            return std::make_shared<SocketOpenSSL>();
# endif
#else
            errorMsg = "TLS support is not enabled on this platform.";
            return nullptr;
#endif
        }
    }
}
