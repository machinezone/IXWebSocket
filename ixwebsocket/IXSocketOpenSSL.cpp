/*
 *  IXSocketOpenSSL.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 *
 *  Adapted from Satori SDK OpenSSL code.
 */

#include "IXSocketOpenSSL.h"
#include "IXSocketConnect.h"

#include <cassert>
#include <iostream>

#include <openssl/x509v3.h>

#include <fnmatch.h>
#include <errno.h>
#define socketerrno errno

namespace ix
{
    std::atomic<bool> SocketOpenSSL::_openSSLInitializationSuccessful(false);

    SocketOpenSSL::SocketOpenSSL(int fd) : Socket(fd),
        _ssl_connection(nullptr),
        _ssl_context(nullptr)
    {
        std::call_once(_openSSLInitFlag, &SocketOpenSSL::openSSLInitialize, this);
    }

    SocketOpenSSL::~SocketOpenSSL()
    {
        SocketOpenSSL::close();
    }

    void SocketOpenSSL::openSSLInitialize()
    {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
        if (!OPENSSL_init_ssl(OPENSSL_INIT_LOAD_CONFIG, nullptr)) return;
#else
        (void) OPENSSL_config(nullptr);
#endif

        (void) OpenSSL_add_ssl_algorithms();
        (void) SSL_load_error_strings();

        _openSSLInitializationSuccessful = true;
    }

    std::string SocketOpenSSL::getSSLError(int ret)
    {
        unsigned long e;

        int err = SSL_get_error(_ssl_connection, ret);

        if (err == SSL_ERROR_WANT_CONNECT || err == SSL_ERROR_WANT_ACCEPT)
        {
            return "OpenSSL failed - connection failure";
        }
        else if (err == SSL_ERROR_WANT_X509_LOOKUP)
        {
            return "OpenSSL failed - x509 error";
        }
        else if (err == SSL_ERROR_SYSCALL)
        {
            e = ERR_get_error();
            if (e > 0)
            {
                std::string errMsg("OpenSSL failed - ");
                errMsg += ERR_error_string(e, nullptr);
                return errMsg;
            }
            else if (e == 0 && ret == 0)
            {
                return "OpenSSL failed - received early EOF";
            }
            else
            {
                return "OpenSSL failed - underlying BIO reported an I/O error";
            }
        }
        else if (err == SSL_ERROR_SSL)
        {
            e = ERR_get_error();
            std::string errMsg("OpenSSL failed - ");
            errMsg += ERR_error_string(e, nullptr);
            return errMsg;
        }
        else if (err == SSL_ERROR_NONE)
        {
            return "OpenSSL failed - err none";
        }
        else if (err == SSL_ERROR_ZERO_RETURN)
        {
            return "OpenSSL failed - err zero return";
        }
        else
        {
            return "OpenSSL failed - unknown error";
        }
    }

    SSL_CTX* SocketOpenSSL::openSSLCreateContext(std::string& errMsg)
    {
        const SSL_METHOD* method = SSLv23_client_method();
        if (method == nullptr)
        {
            errMsg = "SSLv23_client_method failure";
            return nullptr;
        }
        _ssl_method = method;

        SSL_CTX* ctx = SSL_CTX_new(_ssl_method);
        if (ctx)
        {
            // To skip verification, pass in SSL_VERIFY_NONE
            SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER,
                               [](int preverify, X509_STORE_CTX*) -> int
                               {
                                   return preverify;
                               });

            SSL_CTX_set_verify_depth(ctx, 4);
            SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
        }
        return ctx;
    }

    /**
     * Check whether a hostname matches a pattern
     */
    bool SocketOpenSSL::checkHost(const std::string& host, const char *pattern)
    {
        return fnmatch(pattern, host.c_str(), 0) != FNM_NOMATCH;
    }

    bool SocketOpenSSL::openSSLCheckServerCert(SSL *ssl,
                                               const std::string& hostname,
                                               std::string& errMsg)
    {
        X509 *server_cert = SSL_get_peer_certificate(ssl);
        if (server_cert == nullptr)
        {
            errMsg = "OpenSSL failed - peer didn't present a X509 certificate.";
            return false;
        }

#if OPENSSL_VERSION_NUMBER < 0x10100000L
        // Check server name
        bool hostname_verifies_ok = false;
        STACK_OF(GENERAL_NAME) *san_names =
            (STACK_OF(GENERAL_NAME)*) X509_get_ext_d2i((X509 *)server_cert,
                                                       NID_subject_alt_name, NULL, NULL);
        if (san_names)
        {
            for (int i=0; i<sk_GENERAL_NAME_num(san_names); i++)
            {
                const GENERAL_NAME *sk_name = sk_GENERAL_NAME_value(san_names, i);
                if (sk_name->type == GEN_DNS)
                {
                    char *name = (char *)ASN1_STRING_data(sk_name->d.dNSName);
                    if ((size_t)ASN1_STRING_length(sk_name->d.dNSName) == strlen(name) &&
                        checkHost(hostname, name))
                    {
                        hostname_verifies_ok = true;
                        break;
                    }
                }
            }
        }
        sk_GENERAL_NAME_pop_free(san_names, GENERAL_NAME_free);

        if (!hostname_verifies_ok)
        {
            int cn_pos = X509_NAME_get_index_by_NID(X509_get_subject_name((X509 *)server_cert),
                                                    NID_commonName, -1);
            if (cn_pos)
            {
                X509_NAME_ENTRY *cn_entry = X509_NAME_get_entry(
                    X509_get_subject_name((X509 *)server_cert), cn_pos);

                if (cn_entry)
                {
                    ASN1_STRING *cn_asn1 = X509_NAME_ENTRY_get_data(cn_entry);
                    char *cn = (char *)ASN1_STRING_data(cn_asn1);

                    if ((size_t)ASN1_STRING_length(cn_asn1) == strlen(cn) &&
                       checkHost(hostname, cn))
                    {
                        hostname_verifies_ok = true;
                    }
                }
            }
        }

        if (!hostname_verifies_ok)
        {
            errMsg = "OpenSSL failed - certificate was issued for a different domain.";
            return false;
        }
#endif

        X509_free(server_cert);
        return true;
    }

    bool SocketOpenSSL::openSSLHandshake(const std::string& host, std::string& errMsg)
    {
        while (true)
        {
            if (_ssl_connection == nullptr || _ssl_context == nullptr)
            {
                return false;
            }

            ERR_clear_error();
            int connect_result = SSL_connect(_ssl_connection);
            if (connect_result == 1)
            {
                return openSSLCheckServerCert(_ssl_connection, host, errMsg);
            }
            int reason = SSL_get_error(_ssl_connection, connect_result);

            bool rc = false;
            if (reason == SSL_ERROR_WANT_READ || reason == SSL_ERROR_WANT_WRITE)
            {
                rc = true;
            }
            else
            {
                errMsg = getSSLError(connect_result);
                rc = false;
            }

            if (!rc)
            {
                return false;
            }
        }
    }

    // No wait support
    bool SocketOpenSSL::connect(const std::string& host,
                                int port,
                                std::string& errMsg,
                                const CancellationRequest& isCancellationRequested)
    {
        bool handshakeSuccessful = false;
        {
            std::lock_guard<std::mutex> lock(_mutex);

            if (!_openSSLInitializationSuccessful)
            {
                errMsg = "OPENSSL_init_ssl failure";
                return false;
            }

            _sockfd = SocketConnect::connect(host, port, errMsg, isCancellationRequested);
            if (_sockfd == -1) return false;

            _ssl_context = openSSLCreateContext(errMsg);
            if (_ssl_context == nullptr)
            {
                return false;
            }

            ERR_clear_error();
            int cert_load_result = SSL_CTX_set_default_verify_paths(_ssl_context);
            if (cert_load_result == 0)
            {
                unsigned long ssl_err = ERR_get_error();
                errMsg = "OpenSSL failed - SSL_CTX_default_verify_paths loading failed: ";
                errMsg += ERR_error_string(ssl_err, nullptr);
            }

            _ssl_connection = SSL_new(_ssl_context);
            if (_ssl_connection == nullptr)
            {
                errMsg = "OpenSSL failed to connect";
                SSL_CTX_free(_ssl_context);
                _ssl_context = nullptr;
                return false;
            }
            SSL_set_fd(_ssl_connection, _sockfd);

            // SNI support
            SSL_set_tlsext_host_name(_ssl_connection, host.c_str());

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
            // Support for server name verification
            // (The docs say that this should work from 1.0.2, and is the default from
            // 1.1.0, but it does not. To be on the safe side, the manual test below is
            // enabled for all versions prior to 1.1.0.)
            X509_VERIFY_PARAM *param = SSL_get0_param(_ssl_connection);
            X509_VERIFY_PARAM_set1_host(param, host.c_str(), 0);
#endif

            handshakeSuccessful = openSSLHandshake(host, errMsg);
        }

        if (!handshakeSuccessful)
        {
            close();
            return false;
        }

        return true;
    }

    void SocketOpenSSL::close()
    {
        std::lock_guard<std::mutex> lock(_mutex);

        if (_ssl_connection != nullptr)
        {
            SSL_free(_ssl_connection);
            _ssl_connection = nullptr;
        }
        if (_ssl_context != nullptr)
        {
            SSL_CTX_free(_ssl_context);
            _ssl_context = nullptr;
        }

        Socket::close();
    }

    ssize_t SocketOpenSSL::send(char* buf, size_t nbyte)
    {
        ssize_t sent = 0;

        while (nbyte > 0)
        {
            std::lock_guard<std::mutex> lock(_mutex);

            if (_ssl_connection == nullptr || _ssl_context == nullptr)
            {
                return 0;
            }

            ERR_clear_error();
            ssize_t write_result = SSL_write(_ssl_connection, buf + sent, (int) nbyte);
            int reason = SSL_get_error(_ssl_connection, write_result);

            if (reason == SSL_ERROR_NONE) {
                nbyte -= write_result;
                sent += write_result;
            } else if (reason == SSL_ERROR_WANT_READ || reason == SSL_ERROR_WANT_WRITE) {
                errno = EWOULDBLOCK;
                return -1;
            } else {
                return -1;
            }
        }
        return sent;
    }

    ssize_t SocketOpenSSL::send(const std::string& buffer)
    {
        return send((char*)&buffer[0], buffer.size());
    }

    // No wait support
    ssize_t SocketOpenSSL::recv(void* buf, size_t nbyte)
    {
        while (true)
        {
            std::lock_guard<std::mutex> lock(_mutex);

            if (_ssl_connection == nullptr || _ssl_context == nullptr)
            {
                return 0;
            }

            ERR_clear_error();
            ssize_t read_result = SSL_read(_ssl_connection, buf, (int) nbyte);

            if (read_result > 0)
            {
                return read_result;
            }

            int reason = SSL_get_error(_ssl_connection, read_result);

            if (reason == SSL_ERROR_WANT_READ || reason == SSL_ERROR_WANT_WRITE)
            {
                errno = EWOULDBLOCK;
            }
            return -1;
        }
    }

}
