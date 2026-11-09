/*
 *  IXWebSocketSendBufferTest.h
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#pragma once

#include <algorithm>
#include <catch_amalgamated.hpp>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXWebSocketTransport.h>
#include <vector>

namespace ix
{
    struct WebSocketTransportTestAccess
    {
        static void install(WebSocketTransport& transport, std::unique_ptr<Socket> socket)
        {
            transport._socket = std::move(socket);
            transport.setReadyState(WebSocketTransport::ReadyState::OPEN);
        }

        static void enqueue(WebSocketTransport& transport, const std::vector<unsigned char>& bytes)
        {
            std::lock_guard<std::mutex> lock(transport._txbufMutex);
            transport._txbuf.insert(transport._txbuf.end(), bytes.begin(), bytes.end());
        }

        static bool flush(WebSocketTransport& transport)
        {
            return transport.sendOnSocket();
        }

        static void reset(WebSocketTransport& transport)
        {
            transport.resetConnectionState();
        }
    };
} // namespace ix

namespace
{
    // Check the input contract at the actual backend boundary, even for TLS
    // implementations that happen to tolerate a larger retry buffer.
    class TrackedWriteSocket final : public ix::Socket
    {
    public:
        explicit TrackedWriteSocket(ix::Socket& socket)
            : _socket(socket)
        {
        }

        std::ptrdiff_t send(char* buffer, size_t length) override
        {
            if (!_pending.empty())
            {
                CHECK(buffer == _address);
                CHECK(length == _pending.size());
                if (length == _pending.size())
                    CHECK(std::equal(_pending.begin(),
                                     _pending.end(),
                                     reinterpret_cast<unsigned char*>(buffer)));
                ++retries;
            }
            const auto count = _socket.send(buffer, length);
            if (count < 0 && ix::Socket::isWaitNeeded())
            {
                _address = buffer;
                _pending.assign(buffer, buffer + length);
            }
            else
            {
                _pending.clear();
            }
            return count;
        }

        void close() override
        {
            _socket.close();
        }

        unsigned retries = 0;

    private:
        ix::Socket& _socket;
        const char* _address = nullptr;
        std::vector<unsigned char> _pending;
    };

    void installWriteSocket(ix::WebSocketTransport& transport, ix::Socket& socket)
    {
        ix::WebSocketTransportTestAccess::install(
            transport, std::unique_ptr<ix::Socket>(new TrackedWriteSocket(socket)));
    }

    std::vector<unsigned char> writeTestMessage(unsigned char seed, size_t size = 32776)
    {
        std::vector<unsigned char> data(size);
        for (size_t i = 0; i < data.size(); ++i)
            data[i] = static_cast<unsigned char>(seed + i * 31 + i / 251);
        return data;
    }

    void flushWriteTest(ix::WebSocketTransport& transport)
    {
        REQUIRE(ix::WebSocketTransportTestAccess::flush(transport));
        REQUIRE(transport.bufferedAmount() == 0);
    }
} // namespace
