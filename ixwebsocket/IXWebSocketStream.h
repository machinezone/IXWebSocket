/* Copyright (c) 2026 Machine Zone, Inc. All rights reserved. */
#pragma once
#include "IXSocket.h"

namespace ix
{
    // The WebSocket byte stream. Implementations own their transport and use
    // Socket's would-block convention, without exposing a descriptor or connect API.
    class WebSocketStream
    {
    public:
        virtual ~WebSocketStream() = default;
        virtual std::ptrdiff_t send(char* buffer, size_t length) = 0;
        virtual std::ptrdiff_t recv(void* buffer, size_t length) = 0;
        virtual PollResultType isReadyToRead(int timeoutMs) = 0;
        virtual PollResultType isReadyToWrite(int timeoutMs) = 0;
        virtual bool wakeUpFromPoll(uint64_t code) = 0;
        virtual bool isWakeUpFromPollSupported() = 0;
        virtual void close() = 0;
    };

    // HTTP/1.1 Upgrade leaves WebSocket bytes directly on the TCP/TLS socket.
    class SocketStream final : public WebSocketStream
    {
    public:
        explicit SocketStream(std::unique_ptr<Socket> socket);
        std::ptrdiff_t send(char* buffer, size_t length) override;
        std::ptrdiff_t recv(void* buffer, size_t length) override;
        PollResultType isReadyToRead(int timeoutMs) override;
        PollResultType isReadyToWrite(int timeoutMs) override;
        bool wakeUpFromPoll(uint64_t code) override;
        bool isWakeUpFromPollSupported() override;
        void close() override;

    private:
        std::unique_ptr<Socket> _socket;
    };
} // namespace ix
