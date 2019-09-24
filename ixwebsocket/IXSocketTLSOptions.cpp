/*
 *  IXSocketTLSOptions.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXSocketTLSOptions.h"

#include <assert.h>
#include <fstream>

namespace ix
{
    const SocketTLSOptions&& SocketTLSOptions::validated() const
    {
#ifndef IXWEBSOCKET_USE_TLS
        assert(false && "To use TLS features the library must be compiled with USE_TLS");
#endif
        assert((certFile.empty() || std::ifstream(certFile)) &&
               ("certFile not found: " + certFile).c_str());
        assert((keyFile.empty() || !!std::ifstream(keyFile)) &&
               ("keyFile not found: " + keyFile).c_str());
        assert((caFile.empty() || caFile == "NONE" || caFile == "DEFAULT" ||
                !!std::ifstream(caFile)) &&
               ("caFile not found: " + caFile).c_str());

        assert(certFile.empty() == keyFile.empty() &&
               "certFile and keyFile must be both present, or both absent");

        return std::move(*this);
    }

    bool SocketTLSOptions::hasCertAndKey() const
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

    bool SocketTLSOptions::isUsingDefaultCiphers() const
    {
        return ciphers.empty() || ciphers == "DEFAULT";
    }

    void TLSConfigurable::setTLSOptions(const SocketTLSOptions& tlsOptions)
    {
        _tlsOptions = tlsOptions.validated();
    };

} // namespace ix
