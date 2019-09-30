/*
 *  IXSocketTLSOptions.h
 *  Author: Matt DeBoer
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <string>

namespace ix
{
    struct SocketTLSOptions
    {
    public:
        // check validity of the object
        bool isValid() const;

        // the certificate presented to peers
        std::string certFile;
        // the key used for signing/encryption
        std::string keyFile;
        // the ca certificate (or certificate bundle) file containing
        // certificates to be trusted by peers; use 'SYSTEM' to
        // leverage the system defaults, use 'NONE' to disable peer verification
        std::string caFile = "SYSTEM";

        std::string ciphers = "DEFAULT";

        bool hasCertAndKey() const;

        bool isUsingSystemDefaults() const;

        bool isPeerVerifyDisabled() const;

        bool isUsingDefaultCiphers() const;

        const std::string& getErrorMsg() const;

    private:
        mutable std::string _errMsg;
        mutable bool _validated;
    };
} // namespace ix
