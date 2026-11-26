/* Copyright (c) 2026 Machine Zone, Inc. All rights reserved. */
#include "IXHttp2Stream.h"

namespace ix
{
    Http2Stream::Http2Stream(std::unique_ptr<Http2Connection> transport)
        : _connection(std::move(transport))
    {
    }

    std::ptrdiff_t Http2Stream::send(char* buffer, size_t length)
    {
        return _connection->send(buffer, length);
    }

    std::ptrdiff_t Http2Stream::recv(void* buffer, size_t length)
    {
        return _connection->recv(buffer, length);
    }

    PollResultType Http2Stream::isReadyToRead(int timeoutMs)
    {
        return _connection->isReadyToRead(timeoutMs);
    }

    PollResultType Http2Stream::isReadyToWrite(int timeoutMs)
    {
        return _connection->isReadyToWrite(timeoutMs);
    }

    bool Http2Stream::wakeUpFromPoll(uint64_t code)
    {
        return _connection->wakeUpFromPoll(code);
    }

    bool Http2Stream::isWakeUpFromPollSupported()
    {
        return _connection->isWakeUpFromPollSupported();
    }

    void Http2Stream::close()
    {
        _connection->close();
    }
} // namespace ix
