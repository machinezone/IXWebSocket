/*
 *  IXSocketOpenSSL.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXSocket.h"
#include "IXCancellationRequest.h"

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
        SocketOpenSSL(int fd = -1);
        ~SocketOpenSSL();

        virtual bool connect(const std::string& host,
                             int port,
                             std::string& errMsg,
                             const CancellationRequest& isCancellationRequested) final;
        virtual void close() final;

        virtual ssize_t send(char* buffer, size_t length) final;
        virtual ssize_t send(const std::string& buffer) final;
        virtual ssize_t recv(void* buffer, size_t length) final;

    private:
        void openSSLInitialize();
        std::string getSSLError(int ret);
        SSL_CTX* openSSLCreateContext(std::string& errMsg);
        bool openSSLHandshake(const std::string& hostname, std::string& errMsg);
        bool openSSLCheckServerCert(SSL *ssl,
                                    const std::string& hostname,
                                    std::string& errMsg);
        bool checkHost(const std::string& host, const char *pattern);

        SSL* _ssl_connection;
        SSL_CTX* _ssl_context;
        const SSL_METHOD* _ssl_method;
        mutable std::mutex _mutex;  // OpenSSL routines are not thread-safe

        std::once_flag _openSSLInitFlag;
        static std::atomic<bool> _openSSLInitializationSuccessful;
    };

}
