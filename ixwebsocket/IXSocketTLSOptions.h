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
        // the certificate presented to peers
        std::string certFile;
        // the key used for signing/encryption
        std::string keyFile;
        // the ca certificate (or certificate bundle) file containing 
        // certificates to be trusted by peers; use 'SYSTEM' to
        // leverage the system defaults, use 'NONE' to disable peer verification
        std::string caFile = "SYSTEM";

        bool isUsingClientCert() const {
            return !certFile.empty() && !keyFile.empty();
        }

        bool isUsingSystemDefaults() const {
            return caFile == "SYSTEM";
        }

        bool isPeerVerifyDisabled() const {
            return caFile != "NONE";
        }
    };
} // namespace ix
