/*
 *  IXSocketTLSOptions.h
 *  Author: Matt DeBoer
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXSocketTLSOptions.h"

#include <assert.h>
#include <fstream>
#include <sstream>

namespace ix
{
    const char* kTLSCAFileUseSystemDefaults = "SYSTEM";
    const char* kTLSCAFileDisableVerify = "NONE";
    const char* kTLSCiphersUseDefault = "DEFAULT";
    const char* kTLSInMemoryMarker = "-----BEGIN CERTIFICATE-----";

    bool SocketTLSOptions::isValid() const
    {
        if (!_validated)
        {
            if (!certFile.empty() && !isUsingInMemoryCert() && !std::ifstream(certFile))
            {
                _errMsg = "certFile not found: " + certFile;
                return false;
            }
            if (!keyFile.empty() && !isUsingInMemoryKey() && !std::ifstream(keyFile))
            {
                _errMsg = "keyFile not found: " + keyFile;
                return false;
            }
            if (!caFile.empty() && caFile != kTLSCAFileDisableVerify &&
                caFile != kTLSCAFileUseSystemDefaults && !isUsingInMemoryCAs() &&
                !std::ifstream(caFile))
            {
                _errMsg = "caFile not found: " + caFile;
                return false;
            }

            if (certFile.empty() != keyFile.empty())
            {
                _errMsg = "certFile and keyFile must be both present, or both absent";
                return false;
            }

            _validated = true;
        }
        return true;
    }

    bool SocketTLSOptions::hasCertAndKey() const
    {
        return !certFile.empty() && !keyFile.empty();
    }

    bool SocketTLSOptions::isUsingSystemDefaults() const
    {
        return caFile == kTLSCAFileUseSystemDefaults;
    }

    bool SocketTLSOptions::isUsingInMemoryCAs() const
    {
        return caFile.find(kTLSInMemoryMarker) != std::string::npos;
    }

    bool SocketTLSOptions::isUsingInMemoryCert() const
    {
        return certFile.find(kTLSInMemoryMarker) != std::string::npos;
    }

    bool SocketTLSOptions::isUsingInMemoryKey() const
    {
        return keyFile.find("-----BEGIN ") != std::string::npos &&
               keyFile.find("PRIVATE KEY-----") != std::string::npos;
    }

    bool SocketTLSOptions::isPeerVerifyDisabled() const
    {
        return caFile == kTLSCAFileDisableVerify;
    }

    bool SocketTLSOptions::isUsingDefaultCiphers() const
    {
        return ciphers.empty() || ciphers == kTLSCiphersUseDefault;
    }

    const std::string& SocketTLSOptions::getErrorMsg() const
    {
        return _errMsg;
    }

    std::string SocketTLSOptions::getDescription() const
    {
        std::ostringstream oss;
        oss << "TLS Options:" << std::endl;
        oss << "  certFile = " << (isUsingInMemoryCert() ? "<in-memory PEM>" : certFile)
            << std::endl;
        oss << "  keyFile  = " << (isUsingInMemoryKey() ? "<in-memory PEM>" : keyFile) << std::endl;
        oss << "  keyLogFile = " << keyLogFile << std::endl;
        oss << "  caFile   = " << (isUsingInMemoryCAs() ? "<in-memory PEM>" : caFile) << std::endl;
        oss << "  ciphers  = " << ciphers << std::endl;
        oss << "  tls      = " << tls << std::endl;
        return oss.str();
    }
} // namespace ix
