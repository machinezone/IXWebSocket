/*
 *  IXSocketOpenSSL.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXSocket.h"

#include <openssl/bio.h>
#include <openssl/hmac.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <mutex>

namespace ix 
{
    class SocketOpenSSL : public Socket 
    {
    public:
        SocketOpenSSL();
        ~SocketOpenSSL();

        virtual bool connect(const std::string& host, 
                             int port,
                             std::string& errMsg) final;
        virtual void close() final;

        virtual int send(char* buffer, size_t length) final;
        virtual int send(const std::string& buffer) final;
        virtual int recv(void* buffer, size_t length) final;

    private:
        std::string getSSLError(int ret);
        SSL_CTX* openSSLCreateContext(std::string& errMsg);
        bool openSSLHandshake(const std::string& hostname, std::string& errMsg);
        bool openSSLCheckServerCert(SSL *ssl,
                                    const std::string& hostname,
                                    std::string& errMsg);
        bool checkHost(const std::string& host, const char *pattern);

        SSL_CTX* _ssl_context;
        SSL* _ssl_connection;
        const SSL_METHOD* _ssl_method;
        mutable std::mutex _mutex;  // OpenSSL routines are not thread-safe
    };

}
