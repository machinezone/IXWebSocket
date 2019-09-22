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
        std::string cert_file;
        // the key used for signing/encryption
        std::string key_file;
        // the ca certificate (or certificate bundle) file containing 
        // certificates to be trusted by peers; use 'SYSTEM' to
        // leverage the system defaults, use 'NONE' to disable peer verification
        std::string ca_file = "SYSTEM";

        bool isUsingClientCert() const {
            return !cert_file.empty() && !key_file.empty();
        }

        bool isUsingSystemDefaults() const {
            return ca_file == "SYSTEM";
        }

        bool isPeerVerifyDisabled() const {
            return ca_file != "NONE";
        }
    };
} // namespace ix
