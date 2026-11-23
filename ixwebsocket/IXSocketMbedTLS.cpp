/*
 *  IXSocketMbedTLS.cpp
 *  Author: Benjamin Sergeant, Max Weisel
 *  Copyright (c) 2019-2020 Machine Zone, Inc. All rights reserved.
 *
 *  Some code taken from
 *  https://github.com/rottor12/WsClientLib/blob/master/lib/src/WsClientLib.cpp
 *  and mini_client.c example from mbedtls
 */
#ifdef IXWEBSOCKET_USE_MBED_TLS

#include "IXSocketMbedTLS.h"

#include "IXNetSystem.h"
#include "IXSocket.h"
#include "IXSocketConnect.h"
#include <cstdint>
#include <string.h>

#ifdef _WIN32
// For manipulating the certificate store
#include <wincrypt.h>
#endif

namespace ix
{
    SocketMbedTLS::SocketMbedTLS(const SocketTLSOptions& tlsOptions, int fd)
        : Socket(fd)
        , _tlsOptions(tlsOptions)
    {
        initMBedTLS();
    }

    SocketMbedTLS::~SocketMbedTLS()
    {
        SocketMbedTLS::close();
    }

    void SocketMbedTLS::initMBedTLS()
    {
        std::lock_guard<std::mutex> lock(_mutex);

        if (!_mbedtlsInitialized)
        {
            _mbedtlsInitialized = true;
            mbedtls_ssl_init(&_ssl);
            mbedtls_ssl_config_init(&_conf);
#if MBEDTLS_VERSION_MAJOR < 4
            mbedtls_ctr_drbg_init(&_ctr_drbg);
            mbedtls_entropy_init(&_entropy);
#endif
            mbedtls_x509_crt_init(&_cacert);
            mbedtls_x509_crt_init(&_cert);
            mbedtls_pk_init(&_pkey);
        }
        // Initialize the PSA Crypto API for mbedTLS 3.6+ and all 4.x releases.
        // See: https://github.com/Mbed-TLS/mbedtls/blob/development/docs/use-psa-crypto.md
#if MBEDTLS_VERSION_MAJOR >= 4 || (MBEDTLS_VERSION_MAJOR == 3 && MBEDTLS_VERSION_MINOR >= 6)
        psa_crypto_init();
#endif
    }

    bool SocketMbedTLS::loadSystemCertificates(std::string& errorMsg)
    {
#ifdef _WIN32
        DWORD flags = CERT_STORE_READONLY_FLAG | CERT_STORE_OPEN_EXISTING_FLAG |
                      CERT_SYSTEM_STORE_CURRENT_USER;
        HCERTSTORE systemStore = CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, 0, flags, L"Root");

        if (!systemStore)
        {
            errorMsg = "CertOpenStore failed with ";
            errorMsg += std::to_string(GetLastError());
            return false;
        }

        PCCERT_CONTEXT certificateIterator = NULL;

        int certificateCount = 0;
        while (certificateIterator = CertEnumCertificatesInStore(systemStore, certificateIterator))
        {
            if (certificateIterator->dwCertEncodingType & X509_ASN_ENCODING)
            {
                int ret = mbedtls_x509_crt_parse(&_cacert,
                                                 certificateIterator->pbCertEncoded,
                                                 certificateIterator->cbCertEncoded);
                if (ret == 0)
                {
                    ++certificateCount;
                }
            }
        }

        CertFreeCertificateContext(certificateIterator);
        CertCloseStore(systemStore, 0);

        if (certificateCount == 0)
        {
            errorMsg = "No certificates found";
            return false;
        }

        return true;
#else
        errorMsg = "Mbed TLS does not support caFile=SYSTEM on this platform; "
                   "set caFile to a trusted PEM CA bundle";
        return false;
#endif
    }

    bool SocketMbedTLS::init(const std::string& host, bool isClient, std::string& errMsg)
    {
        initMBedTLS();
        std::lock_guard<std::mutex> lock(_mutex);

#if MBEDTLS_VERSION_MAJOR >= 4 || (MBEDTLS_VERSION_MAJOR == 3 && MBEDTLS_VERSION_MINOR >= 6)
        psa_status_t status = psa_crypto_init();
        if (status != PSA_SUCCESS)
        {
            errMsg = "PSA crypto init failed: " + std::to_string((int) status);
            return false;
        }
#endif

#if MBEDTLS_VERSION_MAJOR < 4
        const char* pers = "IXSocketMbedTLS";
        if (mbedtls_ctr_drbg_seed(&_ctr_drbg,
                                  mbedtls_entropy_func,
                                  &_entropy,
                                  (const unsigned char*) pers,
                                  strlen(pers)) != 0)
        {
            errMsg = "Setting entropy seed failed";
            return false;
        }
#endif

        if (mbedtls_ssl_config_defaults(&_conf,
                                        (isClient) ? MBEDTLS_SSL_IS_CLIENT : MBEDTLS_SSL_IS_SERVER,
                                        MBEDTLS_SSL_TRANSPORT_STREAM,
                                        MBEDTLS_SSL_PRESET_DEFAULT) != 0)
        {
            errMsg = "Setting config default failed";
            return false;
        }

#if MBEDTLS_VERSION_MAJOR < 4
        mbedtls_ssl_conf_rng(&_conf, mbedtls_ctr_drbg_random, &_ctr_drbg);
#endif

        if (_tlsOptions.certFile.empty() != _tlsOptions.keyFile.empty())
        {
            errMsg = "certFile and keyFile must be both present, or both absent";
            return false;
        }
        if (_tlsOptions.hasCertAndKey())
        {
            const int certResult =
                _tlsOptions.isUsingInMemoryCert()
                    ? mbedtls_x509_crt_parse(
                          &_cert,
                          reinterpret_cast<const unsigned char*>(_tlsOptions.certFile.c_str()),
                          _tlsOptions.certFile.size() + 1)
                    : mbedtls_x509_crt_parse_file(&_cert, _tlsOptions.certFile.c_str());
            if (certResult != 0)
            {
                errMsg = "Cannot parse certificate chain";
                return false;
            }
            int keyResult;
            if (_tlsOptions.isUsingInMemoryKey())
            {
                const auto* key =
                    reinterpret_cast<const unsigned char*>(_tlsOptions.keyFile.c_str());
                const size_t size = _tlsOptions.keyFile.size() + 1;
#if MBEDTLS_VERSION_MAJOR == 3
                keyResult = mbedtls_pk_parse_key(
                    &_pkey, key, size, nullptr, 0, mbedtls_ctr_drbg_random, &_ctr_drbg);
#else
                keyResult = mbedtls_pk_parse_key(&_pkey, key, size, nullptr, 0);
#endif
            }
            else
            {
#if MBEDTLS_VERSION_MAJOR == 3
                keyResult = mbedtls_pk_parse_keyfile(
                    &_pkey, _tlsOptions.keyFile.c_str(), "", mbedtls_ctr_drbg_random, &_ctr_drbg);
#else
                keyResult = mbedtls_pk_parse_keyfile(&_pkey, _tlsOptions.keyFile.c_str(), "");
#endif
            }
            if (keyResult < 0)
            {
                errMsg = "Cannot parse private key";
                return false;
            }
            if (mbedtls_ssl_conf_own_cert(&_conf, &_cert, &_pkey) < 0)
            {
                errMsg = "Problem configuring certificate/private key";
                return false;
            }
        }

        if (_tlsOptions.isPeerVerifyDisabled())
        {
            mbedtls_ssl_conf_authmode(&_conf, MBEDTLS_SSL_VERIFY_NONE);
        }
        else
        {
            // FIXME: should we call mbedtls_ssl_conf_verify ?
            mbedtls_ssl_conf_authmode(&_conf, MBEDTLS_SSL_VERIFY_REQUIRED);

            if (_tlsOptions.isUsingSystemDefaults())
            {
                if (!loadSystemCertificates(errMsg))
                {
                    return false;
                }
            }
            else
            {
                if (_tlsOptions.isUsingInMemoryCAs())
                {
                    const char* buffer = _tlsOptions.caFile.c_str();
                    size_t bufferSize =
                        _tlsOptions.caFile.size() + 1; // Needs to include null terminating
                                                       // character otherwise mbedtls will fail.
                    if (mbedtls_x509_crt_parse(
                            &_cacert, (const unsigned char*) buffer, bufferSize) < 0)
                    {
                        errMsg = "Cannot parse CA from memory.";
                        return false;
                    }
                }
                else if (mbedtls_x509_crt_parse_file(&_cacert, _tlsOptions.caFile.c_str()) < 0)
                {
                    errMsg = "Cannot parse CA file '" + _tlsOptions.caFile + "'";
                    return false;
                }
            }

            mbedtls_ssl_conf_ca_chain(&_conf, &_cacert, NULL);
        }

        if (isClient && !_tlsOptions.alpnProtocols.empty())
        {
#ifdef MBEDTLS_SSL_ALPN
            _alpnProtocols.clear();
            for (const auto& protocol : _tlsOptions.alpnProtocols)
            {
                _alpnProtocols.push_back(protocol.c_str());
                if (protocol == "h2")
                {
#if MBEDTLS_VERSION_NUMBER >= 0x03020000
                    mbedtls_ssl_conf_min_tls_version(&_conf, MBEDTLS_SSL_VERSION_TLS1_2);
#else
                    mbedtls_ssl_conf_min_version(
                        &_conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
#endif
                }
            }
            _alpnProtocols.push_back(nullptr);
            if (mbedtls_ssl_conf_alpn_protocols(&_conf, _alpnProtocols.data()) != 0)
            {
                errMsg = "Cannot configure ALPN";
                return false;
            }
#else
            errMsg = "This MbedTLS build does not support ALPN";
            return false;
#endif
        }
        if (mbedtls_ssl_setup(&_ssl, &_conf) != 0)
        {
            errMsg = "SSL setup failed";
            return false;
        }

        if (!_tlsOptions.disable_hostname_validation)
        {
            if (!host.empty() && mbedtls_ssl_set_hostname(&_ssl, host.c_str()) != 0)
            {
                errMsg = "SNI setup failed";
                return false;
            }
        }

        return true;
    }

    bool SocketMbedTLS::accept(std::string& errMsg)
    {
        return accept(errMsg, CancellationRequest());
    }

    bool SocketMbedTLS::accept(std::string& errMsg,
                               const CancellationRequest& isCancellationRequested)
    {
        bool isClient = false;
        bool initialized = init(std::string(), isClient, errMsg);
        if (!initialized)
        {
            close();
            return false;
        }

        mbedtls_ssl_set_bio(&_ssl, &_sockfd, mbedtls_net_send, mbedtls_net_recv, NULL);

        return handshake(errMsg, isCancellationRequested);
    }

    bool SocketMbedTLS::connect(const std::string& host,
                                int port,
                                std::string& errMsg,
                                const CancellationRequest& isCancellationRequested)
    {
        _negotiatedProtocol.clear();
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _sockfd = connectToHost(host, port, errMsg, isCancellationRequested);
            if (_sockfd == -1)
            {
                return false;
            }
        }

        bool isClient = true;
        bool initialized = init(host, isClient, errMsg);
        if (!initialized)
        {
            close();
            return false;
        }

        mbedtls_ssl_set_bio(&_ssl, &_sockfd, mbedtls_net_send, mbedtls_net_recv, NULL);

        const bool connected = handshake(errMsg, isCancellationRequested);
#ifdef MBEDTLS_SSL_ALPN
        if (connected)
        {
            const char* selected = mbedtls_ssl_get_alpn_protocol(&_ssl);
            _negotiatedProtocol = selected ? selected : "";
        }
#endif
        return connected;
    }

    bool SocketMbedTLS::handshake(std::string& errMsg,
                                  const CancellationRequest& isCancellationRequested)
    {
        int res;
        do
        {
            if (isCancellationRequested && isCancellationRequested())
            {
                errMsg = "Cancellation requested";
                close();
                return false;
            }
            {
                std::lock_guard<std::mutex> lock(_mutex);
                res = mbedtls_ssl_handshake(&_ssl);
            }
            if (res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                auto ready =
                    res == MBEDTLS_ERR_SSL_WANT_READ ? isReadyToRead(10) : isReadyToWrite(10);
                if (ready == PollResultType::Error)
                {
                    errMsg = "Socket error during TLS handshake";
                    close();
                    return false;
                }
            }
        } while (res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE);

        if (res != 0)
        {
            char buf[256];
            mbedtls_strerror(res, buf, sizeof(buf));

            errMsg = "error in handshake : ";
            errMsg += buf;

            if (res == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED)
            {
                char verifyBuf[512];
                uint32_t flags = mbedtls_ssl_get_verify_result(&_ssl);

                mbedtls_x509_crt_verify_info(verifyBuf, sizeof(verifyBuf), "  ! ", flags);
                errMsg += " : ";
                errMsg += verifyBuf;
            }

            close();
            return false;
        }

        return true;
    }

    void SocketMbedTLS::close()
    {
        std::lock_guard<std::mutex> lock(_mutex);

        mbedtls_ssl_free(&_ssl);
        mbedtls_ssl_config_free(&_conf);
#if MBEDTLS_VERSION_MAJOR < 4
        mbedtls_ctr_drbg_free(&_ctr_drbg);
        mbedtls_entropy_free(&_entropy);
#endif
        mbedtls_x509_crt_free(&_cacert);
        mbedtls_x509_crt_free(&_cert);
        mbedtls_pk_free(&_pkey);
        _mbedtlsInitialized = false;
        // PSA crypto state is process-global; it is released in ix::uninitNetSystem(),
        // not per socket.

        Socket::close();
    }

    std::ptrdiff_t SocketMbedTLS::send(char* buf, size_t nbyte)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        if (nbyte != 0 && buf == nullptr)
        {
            Socket::setErrno(EINVAL);
            return -1;
        }
        const int res = mbedtls_ssl_write(&_ssl, (unsigned char*) buf, nbyte);
        if (res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            Socket::setErrno(EWOULDBLOCK);
            return -1;
        }

        if (res < 0)
        {
            // A previous retry may have left the socket's wait status set.
            Socket::setErrno(EIO);
            return -1;
        }
        return res;
    }

    std::ptrdiff_t SocketMbedTLS::recv(void* buf, size_t nbyte)
    {
        while (true)
        {
            std::lock_guard<std::mutex> lock(_mutex);

            std::ptrdiff_t res = mbedtls_ssl_read(&_ssl, (unsigned char*) buf, (int) nbyte);

            if (res > 0)
            {
                return res;
            }

            // TLS 1.3 (mbedtls 3.6+): the server can send post-handshake messages
            // (NewSessionTicket, early data) which surface as non-fatal "read again"
            // return codes. Retry the read instead of treating them as errors.
#if defined(MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
            if (res == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
            {
                continue;
            }
#endif
#if defined(MBEDTLS_ERR_SSL_RECEIVED_EARLY_DATA)
            if (res == MBEDTLS_ERR_SSL_RECEIVED_EARLY_DATA)
            {
                continue;
            }
#endif

            if (res == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
            {
                Socket::setErrno(0);
                return 0;
            }
            if (res == 0)
            {
                Socket::setErrno(ECONNRESET);
            }

            else if (res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                Socket::setErrno(EWOULDBLOCK);
            }
            else
            {
                Socket::setErrno(EIO);
            }
            return -1;
        }
    }

} // namespace ix

#endif // IXWEBSOCKET_USE_MBED_TLS
