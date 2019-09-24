/*
 *  IXSocketTLSOptions.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <string>

namespace ix
{
    struct SocketTLSOptions
    {
    private:
        mutable std::string _errMsg;
        mutable bool _validated;

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
    };

    // trait class to supply common tls options validation
    class TLSConfigurable
    {
    protected:
        SocketTLSOptions _tlsOptions;

    public:
        // set the provided tls options, returning true if the
        // supplied options are valid. When invalid options are
        // present, false is returned, and 'getErrorMsg()' may be
        // called on the options object to deterimine the cause
        virtual bool setTLSOptions(const SocketTLSOptions& tlsOptions);
    };
} // namespace ix
