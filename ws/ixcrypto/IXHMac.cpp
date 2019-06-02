/*
 *  IXHMac.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#include "IXHMac.h"
#include "IXBase64.h"

#if defined(IXWEBSOCKET_USE_MBED_TLS)
# include <mbedtls/md.h>
#elif defined(__APPLE__)
#  include <ixwebsocket/IXSocketMbedTLS.h>
# include <CommonCrypto/CommonHMAC.h>
#else
# include <openssl/hmac.h>
#endif

namespace ix
{
    std::string hmac(const std::string& data, const std::string& key)
    {
        constexpr size_t hashSize = 16;
        unsigned char hash[hashSize];

#if defined(IXWEBSOCKET_USE_MBED_TLS)
        mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_MD5),
               (unsigned char *) key.c_str(), key.size(),
               (unsigned char *) data.c_str(), data.size(),
               (unsigned char *) &hash);
#elif defined(__APPLE__)
        CCHmac(kCCHmacAlgMD5,
               key.c_str(), key.size(),
               data.c_str(), data.size(),
               &hash);
#else
        HMAC(EVP_md5(),
             key.c_str(), (int) key.size(),
             (unsigned char *) data.c_str(), (int) data.size(),
             (unsigned char *) hash, nullptr);
#endif

        std::string hashString(reinterpret_cast<char*>(hash), hashSize);

        return base64_encode(hashString, (uint32_t) hashString.size());
    }
}
