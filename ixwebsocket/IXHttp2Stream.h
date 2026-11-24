/* Copyright (c) 2026 Machine Zone, Inc. All rights reserved. */
#pragma once
#include "IXHttp2Connection.h"
#include "IXWebSocketStream.h"

namespace ix
{
    // WebSocket bytes carried by the exclusively owned HTTP/2 stream.
    class Http2Stream final : public WebSocketStream
    {
    public:
        explicit Http2Stream(std::unique_ptr<Http2Connection> connection);
        std::ptrdiff_t send(char* buffer, size_t length) override;
        std::ptrdiff_t recv(void* buffer, size_t length) override;
        PollResultType isReadyToRead(int timeoutMs) override;
        PollResultType isReadyToWrite(int timeoutMs) override;
        bool wakeUpFromPoll(uint64_t code) override;
        bool isWakeUpFromPollSupported() override;
        void close() override;

    private:
        std::unique_ptr<Http2Connection> _connection;
    };
} // namespace ix
