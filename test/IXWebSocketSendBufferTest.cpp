/*
 *  IXWebSocketSendBufferTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include "IXWebSocketSendBufferTest.h"

namespace
{
    class PartialWriteSocket final : public ix::Socket
    {
    public:
        std::ptrdiff_t send(char* buffer, size_t length) override
        {
            if (fail)
            {
                setErrno(EIO);
                return -1;
            }
            if (writesBeforeBlocking == 0)
            {
                setErrno(EWOULDBLOCK);
                return -1;
            }
            if (writesBeforeBlocking > 0) --writesBeforeBlocking;
            const size_t count = (std::min) (length, size_t(7));
            received.insert(received.end(), buffer, buffer + count);
            return static_cast<std::ptrdiff_t>(count);
        }

        int writesBeforeBlocking = 1;
        bool fail = false;
        std::vector<unsigned char> received;
    };
} // namespace

TEST_CASE("Pending sends keep their arguments while new bytes are queued", "[send_buffer]")
{
    PartialWriteSocket socket;
    ix::WebSocketTransport transport;
    installWriteSocket(transport, socket);
    auto expected = writeTestMessage(17);
    ix::WebSocketTransportTestAccess::enqueue(transport, expected);
    REQUIRE(ix::WebSocketTransportTestAccess::flush(transport));
    REQUIRE(transport.bufferedAmount() == expected.size() - 7);

    const auto next = writeTestMessage(93, 65536);
    ix::WebSocketTransportTestAccess::enqueue(transport, next);
    expected.insert(expected.end(), next.begin(), next.end());
    REQUIRE(transport.bufferedAmount() == expected.size() - 7);
    REQUIRE(ix::WebSocketTransportTestAccess::flush(transport));
    REQUIRE(ix::WebSocketTransportTestAccess::flush(transport));
    REQUIRE(transport.bufferedAmount() == expected.size() - 7);
    socket.writesBeforeBlocking = -1;
    flushWriteTest(transport);
    CHECK(socket.received == expected);
}

TEST_CASE("Reset discards both pending and newly queued bytes", "[send_buffer]")
{
    PartialWriteSocket socket;
    ix::WebSocketTransport transport;
    installWriteSocket(transport, socket);
    ix::WebSocketTransportTestAccess::enqueue(transport, writeTestMessage(17));
    REQUIRE(ix::WebSocketTransportTestAccess::flush(transport));
    ix::WebSocketTransportTestAccess::enqueue(transport, writeTestMessage(93));
    REQUIRE(transport.bufferedAmount() > 0);
    ix::WebSocketTransportTestAccess::reset(transport);
    REQUIRE(transport.bufferedAmount() == 0);

    // A new connection must not inherit the old socket or its retry state.
    PartialWriteSocket nextSocket;
    nextSocket.writesBeforeBlocking = -1;
    installWriteSocket(transport, nextSocket);
    const auto fresh = writeTestMessage(41, 20);
    ix::WebSocketTransportTestAccess::enqueue(transport, fresh);
    flushWriteTest(transport);
    CHECK(nextSocket.received == fresh);
}

TEST_CASE("Fatal sends discard both pending and newly queued bytes", "[send_buffer]")
{
    PartialWriteSocket socket;
    ix::WebSocketTransport transport;
    installWriteSocket(transport, socket);
    ix::WebSocketTransportTestAccess::enqueue(transport, writeTestMessage(17));
    REQUIRE(ix::WebSocketTransportTestAccess::flush(transport));
    ix::WebSocketTransportTestAccess::enqueue(transport, writeTestMessage(93));
    socket.fail = true;
    CHECK_FALSE(ix::WebSocketTransportTestAccess::flush(transport));
    CHECK(transport.bufferedAmount() == 0);
    CHECK(transport.getReadyState() == ix::WebSocketTransport::ReadyState::CLOSED);
}
