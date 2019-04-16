/*
 *  IXSocketSChannel.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXSocket.h"

namespace ix
{
    class SocketSChannel : public Socket
    {
    public:
        SocketSChannel();
        ~SocketSChannel();

        virtual bool connect(const std::string& host,
                             int port,
                             std::string& errMsg) final;
        virtual void close() final;

        // The important override
        virtual void secureSocket() final;

        virtual ssize_t send(char* buffer, size_t length) final;
        virtual ssize_t send(const std::string& buffer) final;
        virtual ssize_t recv(void* buffer, size_t length) final;

    private:
    };

}
