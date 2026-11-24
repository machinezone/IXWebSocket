/* Copyright (c) 2026 Machine Zone, Inc. All rights reserved. */
#pragma once
#include "IXHpack.h"
#include "IXSocket.h"
#include "IXWebSocketInitResult.h"
#include <deque>

namespace ix
{
    // One RFC 8441 stream (1) on an exclusively owned HTTP/2 connection.
    // The TLS socket owns the descriptor and its poll interruption mechanism.
    class SocketHttp2 final : public Socket
    {
    public:
        explicit SocketHttp2(std::unique_ptr<Socket> socket);
        ~SocketHttp2() override;
        WebSocketInitResult handshake(const Hpack::Headers& headers,
                                      const CancellationRequest& cancelled);
        std::ptrdiff_t send(char* buffer, size_t length) override;
        std::ptrdiff_t recv(void* buffer, size_t length) override;
        PollResultType isReadyToRead(int timeoutMs) override;
        PollResultType isReadyToWrite(int timeoutMs) override;
        bool wakeUpFromPoll(uint64_t code) override;
        bool isWakeUpFromPollSupported() override;
        void close() override;

    private:
        struct Output
        {
            std::string frame;
            size_t offset = 0;
            size_t accepted = 0;
        };

        std::unique_ptr<Socket> _inner;
        std::mutex _mutex;
        Hpack _hpack;
        std::deque<Output> _output;
        std::string _input, _data, _headerBlock, _error;
        size_t _dataOffset = 0, _accepted = 0;
        int64_t _sendConnection = 65535, _sendStream = 65535;
        int64_t _receiveConnection = 65535, _receiveStream = 65535;
        uint32_t _initialWindow = 65535, _maxConcurrent = 0xffffffffu;
        uint32_t _peerHeaderLimit = 0xffffffffu;
        WebSocketHttpHeaders _headers;
        int _status = 0;
        bool _firstFrame = true, _settingsReceived = false, _awaitingAck = true;
        bool _extendedConnect = false, _requestSent = false, _headerActive = false;
        bool _headerEndStream = false, _opened = false, _ended = false;
        bool _dataPending = false, _closed = false, _reset = false, _deferEnd = false;
        bool queue(unsigned type,
                   unsigned flags,
                   uint32_t stream,
                   const std::string& payload,
                   size_t accepted = 0);
        bool flush();
        bool read();
        bool frame(unsigned type, unsigned flags, uint32_t stream, const std::string& payload);
        bool headers();
        bool fail(const std::string& message, uint32_t code = 1);
        void credit(size_t count);
    };
} // namespace ix
