/*
 *  IXHMac.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */
#include "IXHMac.h"
#include "base64.h"

#include <openssl/hmac.h>

namespace ix
{
    std::string hmac(const std::string& data, const std::string& key)
    {
        constexpr size_t hashSize = 16;
        unsigned char hash[hashSize];

        HMAC(EVP_md5(),
             key.c_str(), (int) key.size(),
             (unsigned char *) data.c_str(), (int) data.size(),
             (unsigned char *) hash, nullptr);

        std::string hashString(reinterpret_cast<char*>(hash), hashSize);

        return base64_encode(hashString, (uint32_t) hashString.size());
    }
}
