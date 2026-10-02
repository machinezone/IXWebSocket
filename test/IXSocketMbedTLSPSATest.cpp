/*
 *  IXSocketMbedTLSPSATest.cpp
 *
 *  PSA crypto state is process-global: closing one SocketMbedTLS must not
 *  tear it down under other live sockets.
 */

#ifdef IXWEBSOCKET_USE_MBED_TLS

#include <mbedtls/version.h>

#if MBEDTLS_VERSION_MAJOR >= 4 || (MBEDTLS_VERSION_MAJOR == 3 && MBEDTLS_VERSION_MINOR >= 6)

#include "IXTest.h"
#include <catch_amalgamated.hpp>
#include <iostream>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXSocketMbedTLS.h>
#include <psa/crypto.h>

using namespace ix;

namespace
{
    // Hashing works without PSA init, so probe the RNG and a key slot instead.
    bool psaStateWorks()
    {
        uint8_t bytes[16];
        psa_status_t status = psa_generate_random(bytes, sizeof(bytes));
        if (status != PSA_SUCCESS)
        {
            std::cerr << "psa_generate_random failed: " << (int) status << std::endl;
            return false;
        }

        psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
        psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
        psa_set_key_bits(&attributes, 128);
        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
        psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));

        psa_key_id_t key = 0;
        status = psa_import_key(&attributes, bytes, sizeof(bytes), &key);
        psa_reset_key_attributes(&attributes);
        if (status != PSA_SUCCESS)
        {
            std::cerr << "psa_import_key failed: " << (int) status << std::endl;
            return false;
        }

        return psa_destroy_key(key) == PSA_SUCCESS;
    }
} // namespace

TEST_CASE("SocketMbedTLS close does not tear down process-global PSA state", "[socket_mbedtls]")
{
    SocketMbedTLS a(SocketTLSOptions{});
    REQUIRE(psaStateWorks());

    {
        SocketMbedTLS b(SocketTLSOptions{});
        b.close();
    }
    REQUIRE(psaStateWorks());

    a.close();

    SocketMbedTLS c(SocketTLSOptions{});
    REQUIRE(psaStateWorks());
}

TEST_CASE("uninitNetSystem releases PSA state and allows re-init", "[socket_mbedtls]")
{
    REQUIRE(initNetSystem());
    {
        SocketMbedTLS a(SocketTLSOptions{});
        a.close();
    }
    REQUIRE(uninitNetSystem());

    REQUIRE(psa_crypto_init() == PSA_SUCCESS);
    REQUIRE(psaStateWorks());
}

#endif // MBEDTLS_VERSION_MAJOR >= 3.6

#endif // IXWEBSOCKET_USE_MBED_TLS
