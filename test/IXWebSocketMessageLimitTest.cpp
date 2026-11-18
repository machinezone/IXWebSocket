/*
 *  IXWebSocketMessageLimitTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <catch_amalgamated.hpp>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketPerMessageDeflateCodec.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXWebSocketTransport.h>

namespace ix
{
    struct WebSocketTransportTestAccess
    {
        static void install(WebSocketTransport& transport,
                            std::unique_ptr<Socket> socket,
                            bool compression)
        {
            transport._socket = std::move(socket);
            transport._useMask = false;
            if (compression)
            {
                transport._perMessageDeflate.reset(new WebSocketPerMessageDeflate());
                REQUIRE(
                    transport._perMessageDeflate->init(WebSocketPerMessageDeflateOptions(), true));
            }
            transport.setReadyState(WebSocketTransport::ReadyState::OPEN);
        }
        static void append(WebSocketTransport& transport, const std::string& data)
        {
            transport._rxbuf.insert(transport._rxbuf.end(), data.begin(), data.end());
        }
        static uint64_t wanted(const WebSocketTransport& transport)
        {
            return transport._rxbufWanted;
        }
        static size_t chunks(const WebSocketTransport& transport)
        {
            return transport._chunks.size();
        }
        static size_t decodedCapacity(const WebSocketTransport& transport)
        {
            return transport._decompressedMessage.capacity();
        }
    };
} // namespace ix

namespace
{
    using Transport = ix::WebSocketTransport;
    using Access = ix::WebSocketTransportTestAccess;

    std::string header(unsigned char opcode, uint64_t size)
    {
        std::string result(1, static_cast<char>(opcode));
        if (size < 126)
        {
            result += static_cast<char>(0x80 | size);
        }
        else
        {
            const int bytes = size <= 65535 ? 2 : 8;
            result += static_cast<char>(bytes == 2 ? 0xfe : 0xff);
            for (int shift = (bytes - 1) * 8; shift >= 0; shift -= 8)
            {
                result += static_cast<char>((size >> shift) & 0xff);
            }
        }
        return result + std::string(4, '\0'); // Valid client masking key, all zeros.
    }

    std::string frame(unsigned char opcode, const std::string& payload)
    {
        return header(opcode, payload.size()) + payload;
    }

    class CaptureSocket final : public ix::Socket
    {
    public:
        std::ptrdiff_t send(char* data, size_t size) override
        {
            sent.append(data, size);
            return static_cast<std::ptrdiff_t>(size);
        }
        std::string sent;
    };

    struct Receiver
    {
        Transport transport;
        CaptureSocket* socket;
        std::vector<std::string> messages;
        size_t fragments = 0;

        explicit Receiver(uint64_t limit, bool compression = false)
        {
            transport.configure(
                ix::WebSocketPerMessageDeflateOptions(compression), {}, false, 0, {}, limit);
            std::unique_ptr<CaptureSocket> capture(new CaptureSocket());
            socket = capture.get();
            Access::install(transport, std::move(capture), compression);
        }
        void receive(const std::string& bytes)
        {
            Access::append(transport, bytes);
            transport.dispatch(
                Transport::PollResult::Succeeded,
                [&](const std::string& message, size_t, bool error, Transport::MessageKind kind)
                {
                    CHECK_FALSE(error);
                    if (kind == Transport::MessageKind::MSG_TEXT ||
                        kind == Transport::MessageKind::MSG_BINARY)
                    {
                        messages.push_back(message);
                    }
                    if (kind == Transport::MessageKind::FRAGMENT)
                    {
                        ++fragments;
                    }
                });
        }
        uint16_t closeCode() const
        {
            const auto& bytes = socket->sent;
            for (size_t offset = 0; offset + 4 <= bytes.size();)
            {
                const size_t size = static_cast<unsigned char>(bytes[offset + 1]);
                REQUIRE(size <= 125);
                if ((bytes[offset] & 0xf) == 8)
                {
                    return (static_cast<unsigned char>(bytes[offset + 2]) << 8) |
                           static_cast<unsigned char>(bytes[offset + 3]);
                }
                offset += 2 + size;
            }
            FAIL("No Close frame was sent");
            return 0;
        }
    };
} // namespace

TEST_CASE("Message-size limits default to unlimited", "[websocket][message_limit]")
{
    ix::WebSocket client;
    ix::WebSocketServer server;
    CHECK(client.getMaxMessageSize() == 0);
    CHECK(server.getMaxMessageSize() == 0);
    client.setMaxMessageSize(123);
    server.setMaxMessageSize(456);
    CHECK(client.getMaxMessageSize() == 123);
    CHECK(server.getMaxMessageSize() == 456);
    Receiver receiver(0);
    const uint64_t size = uint64_t(1) << 40;
    receiver.receive(header(0x82, size));
    CHECK(receiver.transport.getReadyState() == Transport::ReadyState::OPEN);
    CHECK(Access::wanted(receiver.transport) == size + 14);
}

TEST_CASE("Oversized declared payloads close before receiving their data",
          "[websocket][message_limit]")
{
    const uint64_t size = GENERATE(uint64_t(9), uint64_t(256), uint64_t(1) << 40);
    Receiver receiver(8);
    receiver.receive(header(0x82, size));
    CHECK(receiver.closeCode() == 1009);
    CHECK(Access::wanted(receiver.transport) == 0);
    CHECK(receiver.messages.empty());
}

TEST_CASE("Fragmented limits include earlier payloads and reset between messages",
          "[websocket][message_limit]")
{
    const bool exceeds = GENERATE(false, true);
    Receiver receiver(8);
    receiver.receive(frame(0x02, "123"));
    // Control frames do not count toward the message limit.
    receiver.receive(frame(0x89, std::string(125, 'p')));
    if (exceeds)
    {
        receiver.receive(header(0x80, 6));
        CHECK(receiver.closeCode() == 1009);
        CHECK(Access::wanted(receiver.transport) == 0);
        CHECK(receiver.messages.empty());
    }
    else
    {
        receiver.receive(frame(0x80, "45678"));
        receiver.receive(frame(0x82, "abcdefgh"));
        REQUIRE(receiver.messages.size() == 2);
        CHECK(receiver.messages[0] == "12345678");
        CHECK(receiver.messages[1] == "abcdefgh");
        CHECK(Access::chunks(receiver.transport) == 0);
    }
}

TEST_CASE("Empty and tiny fragments cannot exhaust memory through chunk metadata",
          "[websocket][message_limit]")
{
    const std::string payload = GENERATE(std::string(), std::string("x"));
    Receiver receiver(4096);
    receiver.receive(frame(0x02, payload));
    for (size_t i = 1; i < 4096; ++i)
    {
        receiver.receive(frame(0x00, payload));
    }
    CHECK(Access::chunks(receiver.transport) == 1);
    CHECK(receiver.fragments == 4096);
    receiver.receive(frame(0x80, ""));
    REQUIRE(receiver.messages.size() == 1);
    CHECK(receiver.messages[0].size() == 4096 * payload.size());
}

TEST_CASE("Oversized control frames cannot bypass payload checks", "[websocket][message_limit]")
{
    Receiver receiver(8);
    receiver.receive(header(0x89, uint64_t(1) << 40));
    CHECK(receiver.closeCode() == 1002);
    CHECK(Access::wanted(receiver.transport) == 0);
}

#ifdef IXWEBSOCKET_USE_ZLIB
TEST_CASE("Compressed messages are bounded while inflating", "[websocket][message_limit]")
{
    const bool fragmented = GENERATE(false, true);
    const size_t size = GENERATE(size_t(64), size_t(65), size_t(1) << 20);
    ix::WebSocketPerMessageDeflateCompressor compressor;
    REQUIRE(compressor.init(15, true));
    std::string encoded;
    REQUIRE(compressor.compress(std::string(size, 'x'), encoded));
    const uint64_t limit = size > 65 ? 2048 : 64;
    REQUIRE(encoded.size() <= limit);
    Receiver receiver(limit, true);
    if (fragmented)
    {
        const size_t half = encoded.size() / 2;
        receiver.receive(frame(0x42, encoded.substr(0, half)));
        receiver.receive(frame(0x80, encoded.substr(half)));
    }
    else
    {
        receiver.receive(frame(0xc2, encoded));
    }
    if (size == 64)
    {
        REQUIRE(receiver.messages.size() == 1);
        CHECK(receiver.messages[0] == std::string(size, 'x'));
    }
    else
    {
        CHECK(receiver.closeCode() == 1009);
        CHECK(receiver.messages.empty());
        // The oversized output was rejected before growing the output string.
        CHECK(Access::decodedCapacity(receiver.transport) < 1024);
    }
}
#endif
