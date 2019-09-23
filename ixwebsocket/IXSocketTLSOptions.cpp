/*
 *  IXSocketTLSOptions.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXSocketTLSOptions.h"

#include <assert.h>

namespace ix
{
    const SocketTLSOptions&& SocketTLSOptions::validated() const
    {
#ifndef IXWEBSOCKET_USE_TLS
        assert(false && "To use TLS features the library must be compiled with USE_TLS");
#endif
        return std::move(*this);
    }

    bool SocketTLSOptions::isUsingClientCert() const
    {
        return !certFile.empty() && !keyFile.empty();
    }

    bool SocketTLSOptions::isUsingSystemDefaults() const
    {
        return caFile == "SYSTEM";
    }

    bool SocketTLSOptions::isPeerVerifyDisabled() const
    {
        return caFile == "NONE";
    }

} // namespace ix
