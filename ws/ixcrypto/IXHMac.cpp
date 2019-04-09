/*
 *  IXHMac.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */
#include "IXHMac.h"
#include "IXBase64.h"

#ifdef __APPLE__
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

#ifdef __APPLE__
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
