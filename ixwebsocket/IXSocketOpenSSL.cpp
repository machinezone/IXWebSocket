/*
 *  IXSocketOpenSSL.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 *
 *  Adapted from Satori SDK OpenSSL code.
 */

#include "IXSocketOpenSSL.h"
#include <cassert>
#include <iostream>

#include <errno.h>
#define socketerrno errno

namespace {

std::mutex initMutex;
bool openSSLInitialized = false;
bool openSSLInitializationSuccessful = false;

bool openSSLInitialize(std::string& errMsg)
{
    std::lock_guard<std::mutex> lock(initMutex);

    if (openSSLInitialized)
    {
        return openSSLInitializationSuccessful;
    }

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    if (!OPENSSL_init_ssl(OPENSSL_INIT_LOAD_CONFIG, nullptr))
    {
        errMsg = "OPENSSL_init_ssl failure";

        openSSLInitializationSuccessful = false;
        openSSLInitialized = true;
        return false;
    }
#else
    (void) OPENSSL_config(nullptr);
#endif

    (void) OpenSSL_add_ssl_algorithms();
    (void) SSL_load_error_strings();

    openSSLInitializationSuccessful = true;
    return true;
}

int openssl_verify_callback(int preverify, X509_STORE_CTX *x509_ctx)
{
    return preverify;
}

/* create new SSL connection state object */
SSL *openssl_create_connection(SSL_CTX *ctx, int socket)
{
    assert(ctx != nullptr);
    assert(socket > 0);

    SSL *ssl = SSL_new(ctx);
    if (ssl)
        SSL_set_fd(ssl, socket);
    return ssl;
}

bool openssl_check_server_cert(SSL *ssl, std::string& errMsg)
{
    X509 *server_cert = SSL_get_peer_certificate(ssl);
    if (server_cert == nullptr)
    {
        errMsg = "OpenSSL failed - peer didn't present a X509 certificate.";
        return false;
    }
    X509_free(server_cert);
    return true;
}

} // anonymous namespace

namespace ix 
{
    SocketOpenSSL::SocketOpenSSL() :
        _ssl_connection(nullptr), 
        _ssl_context(nullptr)
    {
        ;
    }

    SocketOpenSSL::~SocketOpenSSL()
    {
        SocketOpenSSL::close();
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
            SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, openssl_verify_callback);
            SSL_CTX_set_verify_depth(ctx, 4);
            SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
        }
        return ctx;
    }

    bool SocketOpenSSL::openSSLHandshake(std::string& errMsg) 
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
                return openssl_check_server_cert(_ssl_connection, errMsg);
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
                                std::string& errMsg)
    {
        bool handshakeSuccessful = false;
        {
            std::lock_guard<std::mutex> lock(_mutex);

            if (!openSSLInitialize(errMsg))
            {
                return false;
            }

            _sockfd = Socket::hostname_connect(host, port, errMsg);
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

            _ssl_connection = openssl_create_connection(_ssl_context, _sockfd);
            if (nullptr == _ssl_connection)
            {
                errMsg = "OpenSSL failed to connect";
                SSL_CTX_free(_ssl_context);
                _ssl_context = nullptr;
                return false;
            }

            handshakeSuccessful = openSSLHandshake(errMsg);
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

    int SocketOpenSSL::send(char* buf, size_t nbyte)
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
            int write_result = SSL_write(_ssl_connection, buf + sent, (int) nbyte);
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
        return (int) sent;
    }

    int SocketOpenSSL::send(const std::string& buffer)
    {
        return send((char*)&buffer[0], buffer.size());
    }

    // No wait support
    int SocketOpenSSL::recv(void* buf, size_t nbyte)
    {
        while (true)
        {
            std::lock_guard<std::mutex> lock(_mutex);

            if (_ssl_connection == nullptr || _ssl_context == nullptr)
            {
                return 0;
            }

            ERR_clear_error();
            int read_result = SSL_read(_ssl_connection, buf, (int) nbyte);

            if (read_result > 0)
            {
                return read_result;
            }

            int reason = SSL_get_error(_ssl_connection, read_result);

            if (reason == SSL_ERROR_WANT_READ || reason == SSL_ERROR_WANT_WRITE)
            {
                errno = EWOULDBLOCK;
                return -1;
            } else {
                return -1;
            }
        }
    }

}
