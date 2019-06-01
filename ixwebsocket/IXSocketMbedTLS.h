/*
 *  IXSocketMbedTLS.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXSocket.h"

namespace ix
{
    class SocketMbedTLS final : public Socket
    {
    public:
        SocketMbedTLS();
        ~SocketMbedTLS();

        virtual bool SocketMbedTLS::connect(
            const std::string& host,
            int port,
            std::string& errMsg,
            const CancellationRequest& isCancellationRequested) final;
        virtual void close() final;

        virtual ssize_t send(char* buffer, size_t length) final;
        virtual ssize_t send(const std::string& buffer) final;
        virtual ssize_t recv(void* buffer, size_t length) final;

    private:
    };

} // namespace ix
