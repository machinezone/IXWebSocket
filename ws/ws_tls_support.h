/*
 *  ws_tls_support.hpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */
#pragma once

#include <ixwebsocket/IXSocketTLSOptions.h>

namespace ix
{
    class TLSSupport
    {
    public:
        void setTLSOptions(const SocketTLSOptions& tlsOptions);

    protected:
        ix::SocketTLSOptions _tlsOptions;
    };
} // namespace ix
