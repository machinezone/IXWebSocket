/*
 *  ws_tls_support.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include "ws_tls_support.h"

namespace ix
{
    void TLSSupport::setTLSOptions(const SocketTLSOptions& tlsOptions)
    {
        _tlsOptions = tlsOptions.validated();
    }
} // namespace ix
