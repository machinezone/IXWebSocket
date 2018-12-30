/*
 *  IXSocketAppleSSL.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXSocket.h"
#include "IXCancellationRequest.h"

#include <Security/Security.h>
#include <Security/SecureTransport.h>

#include <mutex>

namespace ix 
{
    class SocketAppleSSL : public Socket 
    {
    public:
        SocketAppleSSL(int fd = -1);
        ~SocketAppleSSL();

        virtual bool connect(const std::string& host, 
                             int port,
                             std::string& errMsg,
                             const CancellationRequest& isCancellationRequested) final;
        virtual void close() final;

        virtual int send(char* buffer, size_t length) final;
        virtual int send(const std::string& buffer) final;
        virtual int recv(void* buffer, size_t length) final;

    private:
        SSLContextRef _sslContext;
        mutable std::mutex _mutex;  // AppleSSL routines are not thread-safe
    };

}
