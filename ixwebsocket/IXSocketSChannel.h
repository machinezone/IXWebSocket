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

        virtual int send(char* buffer, size_t length) final;
        virtual int send(const std::string& buffer) final;
        virtual int recv(void* buffer, size_t length) final;

    private:
    };

}
