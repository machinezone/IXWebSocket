/* Copyright (c) 2026 Machine Zone, Inc. All rights reserved. */
#include "IXWebSocketStream.h"

namespace ix
{
    SocketStream::SocketStream(std::unique_ptr<Socket> transport)
        : _socket(std::move(transport))
    {
    }

    std::ptrdiff_t SocketStream::send(char* buffer, size_t length)
    {
        return _socket->send(buffer, length);
    }

    std::ptrdiff_t SocketStream::recv(void* buffer, size_t length)
    {
        return _socket->recv(buffer, length);
    }

    PollResultType SocketStream::isReadyToRead(int timeoutMs)
    {
        return _socket->isReadyToRead(timeoutMs);
    }

    PollResultType SocketStream::isReadyToWrite(int timeoutMs)
    {
        return _socket->isReadyToWrite(timeoutMs);
    }

    bool SocketStream::wakeUpFromPoll(uint64_t code)
    {
        return _socket->wakeUpFromPoll(code);
    }

    bool SocketStream::isWakeUpFromPollSupported()
    {
        return _socket->isWakeUpFromPollSupported();
    }

    void SocketStream::close()
    {
        _socket->close();
    }
} // namespace ix
