/* Copyright (c) 2026 Machine Zone, Inc. All rights reserved. */
#include <catch_amalgamated.hpp>
#include <cstring>
#include <deque>
#include <ixwebsocket/IXHpack.h>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXSocketHttp2.h>
#include <ixwebsocket/IXWebSocket.h>

namespace
{
    std::string hex(const std::string& value)
    {
        std::string result;
        for (size_t i = 0; i < value.size(); i += 2)
        {
            result += static_cast<char>(std::stoul(value.substr(i, 2), nullptr, 16));
        }
        return result;
    }

    std::string frame(unsigned type, unsigned flags, unsigned stream, const std::string& payload)
    {
        std::string result(9, '\0');
        result[0] = char(payload.size() >> 16);
        result[1] = char(payload.size() >> 8);
        result[2] = char(payload.size());
        result[3] = char(type);
        result[4] = char(flags);
        result[8] = char(stream);
        return result + payload;
    }

    class Peer final : public ix::Socket
    {
    public:
        std::deque<std::string> incoming;
        std::string sent;
        size_t fragment = 16384;
        bool partial = false;
        unsigned writes = 0;

        Peer()
        {
            incoming.push_back(frame(4, 0, 0, hex("000800000001")));
            incoming.push_back(frame(4, 1, 0, ""));
            incoming.push_back(frame(1, 4, 1, hex("88")));
        }

        std::ptrdiff_t send(char* data, size_t size) override
        {
            if (partial && ++writes % 2 == 0)
            {
                setErrno(EWOULDBLOCK);
                return -1;
            }
            size = (std::min) (size, partial ? size_t(3) : size);
            sent.append(data, size);
            return static_cast<std::ptrdiff_t>(size);
        }

        std::ptrdiff_t recv(void* data, size_t size) override
        {
            if (incoming.empty())
            {
                setErrno(EWOULDBLOCK);
                return -1;
            }
            auto& bytes = incoming.front();
            size = (std::min) ({size, bytes.size(), fragment});
            std::memcpy(data, bytes.data(), size);
            bytes.erase(0, size);
            if (bytes.empty())
            {
                incoming.pop_front();
            }
            return static_cast<std::ptrdiff_t>(size);
        }

        ix::PollResultType isReadyToRead(int) override
        {
            return incoming.empty() ? ix::PollResultType::Timeout
                                    : ix::PollResultType::ReadyForRead;
        }

        ix::PollResultType isReadyToWrite(int) override
        {
            return ix::PollResultType::ReadyForWrite;
        }

        bool wakeUpFromPoll(uint64_t) override
        {
            return true;
        }
    };

    ix::Hpack::Headers request {{":method", "CONNECT"},
                                {":protocol", "websocket"},
                                {":scheme", "https"},
                                {":authority", "example.com"},
                                {":path", "/"}};
} // namespace

TEST_CASE("HPACK decodes RFC 7541 requests with and without Huffman coding", "[http2][hpack]")
{
    const bool huffman = GENERATE(false, true);
    ix::Hpack decoder;
    ix::Hpack::Headers fields;
    REQUIRE(decoder.decode(hex(huffman ? "828684418cf1e3c2e5f23a6ba0ab90f4ff"
                                       : "828684410f7777772e6578616d706c652e636f6d"),
                           fields));
    REQUIRE(fields.size() == 4);
    CHECK(fields[0] == std::make_pair(std::string(":method"), std::string("GET")));
    CHECK(fields[3].second == "www.example.com");
    // The authority in the second block references the first block's dynamic table.
    REQUIRE(decoder.decode(hex("828684be58086e6f2d6361636865"), fields));
    CHECK(fields[3].second == "www.example.com");
    CHECK(fields[4].second == "no-cache");
    REQUIRE(decoder.decode(ix::Hpack::encode(request), fields));
    CHECK(fields == request);
}

TEST_CASE("HPACK rejects invalid indexes, lengths, padding and EOS", "[http2][hpack]")
{
    const std::string bad = GENERATE(
        "80", "ff", "be", "000561", "000181", "000184ffffffff", "00810000", "3fe21f", "820020");
    ix::Hpack decoder;
    ix::Hpack::Headers fields;
    INFO(bad);
    CHECK_FALSE(decoder.decode(hex(bad), fields));
}

TEST_CASE("HTTP2 handshake validates settings, response headers and frame ordering", "[http2]")
{
    const int failure = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
    auto peer = new Peer;
    if (failure == 0)
    {
        peer->incoming.front() = frame(6, 0, 0, "12345678");
    }
    if (failure == 1)
    {
        peer->incoming.front() = frame(4, 0, 0, "");
    }
    if (failure == 2)
    {
        peer->incoming.front() = frame(4, 0, 0, hex("000800000002"));
    }
    if (failure == 3)
    {
        peer->incoming.back() = hex("004001000000000001");
    }
    if (failure == 4)
    {
        peer->incoming.back() = frame(1, 4, 1, hex("80"));
    }
    if (failure == 5)
    {
        peer->incoming.back() = frame(1, 0, 1, hex("88"));
        peer->incoming.push_back(frame(6, 0, 0, "12345678"));
    }
    if (failure == 6)
    {
        peer->incoming.back() = frame(1, 4, 1, ix::Hpack::encode({{":status", "101"}}));
    }
    if (failure == 7)
    {
        peer->incoming.back() =
            frame(1, 4, 1, ix::Hpack::encode({{":status", "200"}, {"connection", "upgrade"}}));
    }
    if (failure == 8)
    {
        peer->incoming.back() = frame(1, 5, 1, hex("88"));
    }
    if (failure == 9)
    {
        peer->incoming.back() =
            frame(1, 4, 1, ix::Hpack::encode({{":status", "200"}, {"content-length", "0"}}));
    }
    ix::SocketHttp2 stream {std::unique_ptr<ix::Socket>(peer)};
    const auto result = stream.handshake(request, [] { return false; });
    CHECK_FALSE(result.success);
    CHECK_FALSE(result.errorStr.empty());
}

TEST_CASE("HTTP2 preserves byte streams through fragmented reads and partial writes", "[http2]")
{
    const bool fragmented = GENERATE(false, true);
    auto peer = new Peer;
    peer->partial = fragmented;
    peer->fragment = fragmented ? 1 : 16384;
    ix::SocketHttp2 stream {std::unique_ptr<ix::Socket>(peer)};
    REQUIRE(stream.handshake(request, [] { return false; }).success);
    std::string body = "websocket bytes";
    std::ptrdiff_t sent;
    do
    {
        sent = stream.send(&body[0], body.size());
    } while (sent < 0 && ix::Socket::isWaitNeeded());
    CHECK(sent == body.size());
    const auto encoded = frame(0, 0, 1, body);
    CHECK(peer->sent.find(encoded) != std::string::npos);
    CHECK(peer->sent.find(encoded, peer->sent.find(encoded) + 1) == std::string::npos);
    peer->incoming.push_back(frame(0, 8, 1, std::string("\2") + body + "xx"));
    std::string received;
    char buffer[3];
    while (received.size() < body.size())
    {
        const auto count = stream.recv(buffer, sizeof(buffer));
        if (count > 0)
        {
            received.append(buffer, static_cast<size_t>(count));
        }
        else
        {
            REQUIRE(ix::Socket::isWaitNeeded());
        }
    }
    CHECK(received == body);
    peer->incoming.push_back(frame(0, 1, 1, ""));
    while (stream.recv(buffer, sizeof(buffer)) != 0)
    {
        REQUIRE(ix::Socket::isWaitNeeded());
    }
}

TEST_CASE("HTTP2 WebSockets reject plaintext and HTTP1 connection handoff", "[http2]")
{
    ix::WebSocket socket;
    socket.setHttp2(true);
    socket.setOnMessageCallback([](const ix::WebSocketMessagePtr&) { });
    socket.setUrl("ws://127.0.0.1:1/");
    CHECK_FALSE(socket.connect(1).success);
    ix::HttpClient http;
    socket.setUrl("wss://127.0.0.1:1/");
    auto result = socket.connect(http, 1);
    CHECK_FALSE(result.success);
    CHECK(result.errorStr.find("cannot adopt") != std::string::npos);
}

TEST_CASE("HPACK decodes the complete byte alphabet and rejects truncated literals",
          "[http2][hpack]")
{
    const std::string block = hex(
        "4086f2b47e92547fffc803ffc7fffd8fffffe2fffffe3fffffe4fffffe5fffffe6fffffe7fffffe8ffffeaffff"
        "fff3fffffa"
        "7fffffabffffffdfffffebfffffecfffffedfffffeefffffefffffff0ffffff1ffffff2fffffffbfffffcfffff"
        "fd3fffffd7"
        "fffffdbfffffdffffffe3fffffe7fffffebfffffed4fe3f9ffaffcabf1febfafefe7fdfd2cbb00089969b71d79"
        "fb9f7fff20"
        "ffbff3ff50ddbd7f061c58f265cd9f469d5af66dddbf871e5f9cff7ff7fffc3ff9ffe45fff4719242cb34e6e9d"
        "68a6a3d7da"
        "c426defe3cfaf7fffbfe7ffbffdffffffcfffe6ffff4bfff9ffffa3fffd3ffff53fffd5ffffb3fffeb7fffdaff"
        "ffb7ffff73"
        "fffeeffffdeffffebffffbfffffd9ffffdbfffebffffe0ffffeeffffc3ffff8bffff1ffffe4fffee7fffb1ffff"
        "97fffd9fff"
        "fcdffff9fffffbffffdafffeeffff4ffffb7fffee7fffe8ffffd3fffdeffffd5fffeeffffbdffffe1fffdfffff"
        "7fffff5fff"
        "fecffff07fff87fffe0ffff17fffedffff87ffff77fffeffffeaffff8bfffe3ffff93ffff87fffcbffff37ffff"
        "1fffff83ff"
        "ffe1fffebfffe3ffff3fffff2ffffa3ffffd9fffff17ffffc7fffff27ffffdefffffbffffff2fffff8fffffb7f"
        "ff97fff8ff"
        "fffe6fffffc1fffff87ffffe7fffffc5ffffe5fffe4ffff2fffffd1fffff4ffffffefffffe3fffffc9fffff97f"
        "ffb3ffffcf"
        "fffb7fffcdffff4ffff9ffffd1ffffcffffeaffffafffffddffffeffffff4fffff5fffffabffffa7ffffd7ffff"
        "f9bffffecf"
        "ffffb7fffff3fffffe8fffffd3fffffabfffff5fffffff7ffffecfffffdbfffffbbfffff7ffffff0fffffbbf");
    ix::Hpack decoder;
    ix::Hpack::Headers fields;
    REQUIRE(decoder.decode(block, fields));
    REQUIRE(fields.size() == 1);
    REQUIRE(fields[0].second.size() == 256);
    for (unsigned i = 0; i < 256; ++i)
    {
        CHECK(static_cast<unsigned char>(fields[0].second[i]) == i);
    }
    for (size_t i = 1; i < block.size(); ++i)
    {
        ix::Hpack partial;
        REQUIRE_FALSE(partial.decode(block.substr(0, i), fields));
    }
    CHECK_FALSE(decoder.decode(hex("0084ffffffff00"), fields)); // EOS inside a string.
    REQUIRE(decoder.decode(hex("20"), fields));                 // Evict all dynamic entries.
    CHECK_FALSE(decoder.decode(hex("be"), fields));
}

TEST_CASE("HTTP2 validates stream control and receive windows", "[http2]")
{
    const int failure = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
    auto peer = new Peer;
    ix::SocketHttp2 stream {std::unique_ptr<ix::Socket>(peer)};
    REQUIRE(stream.handshake(request, [] { return false; }).success);
    if (failure == 0)
    {
        peer->incoming.push_back(frame(8, 0, 1, hex("00000000")));
    }
    if (failure == 1)
    {
        peer->incoming.push_back(frame(8, 0, 0, hex("7fffffff")));
    }
    if (failure == 2)
    {
        peer->incoming.push_back(frame(8, 0, 2, hex("00000001")));
    }
    if (failure == 3)
    {
        peer->incoming.push_back(frame(0, 0, 0, "bad"));
    }
    if (failure == 4)
    {
        peer->incoming.push_back(frame(0, 8, 1, hex("03")));
    }
    if (failure == 5)
    {
        peer->incoming.push_back(frame(6, 0, 0, "short"));
    }
    if (failure == 6)
    {
        peer->incoming.push_back(frame(5, 0, 1, ""));
    }
    if (failure == 7)
    {
        peer->incoming.push_back(frame(9, 4, 1, ""));
    }
    if (failure == 8)
    {
        peer->incoming.push_back(frame(2, 0, 1, hex("0000000100")));
    }
    if (failure == 9)
    {
        peer->incoming.push_back(frame(3, 0, 1, hex("00000001")));
    }
    char byte;
    CHECK(stream.recv(&byte, 1) <= 0);
    stream.close();
    CHECK(peer->sent.find(hex("000008070000000000")) != std::string::npos); // GOAWAY.
}

TEST_CASE("HTTP2 supports continuation, extension frames and a remote half close", "[http2]")
{
    auto peer = new Peer;
    const auto block = ix::Hpack::encode({{":status", "200"}, {"x-test", "present"}});
    peer->incoming.back() = frame(1, 0, 1, block.substr(0, 1));
    peer->incoming.push_back(frame(9, 4, 1, block.substr(1)));
    ix::SocketHttp2 stream {std::unique_ptr<ix::Socket>(peer)};
    auto result = stream.handshake(request, [] { return false; });
    REQUIRE(result.success);
    CHECK(result.headers["x-test"] == "present");
    peer->incoming.push_back(frame(0xfe, 0, 0, "extension"));
    peer->incoming.push_back(frame(0, 1, 1, "last"));
    char buffer[4];
    CHECK(stream.recv(buffer, sizeof(buffer)) == -1); // Ignored extension frame.
    REQUIRE(stream.recv(buffer, sizeof(buffer)) == 4);
    CHECK(std::string(buffer, 4) == "last");
    CHECK(stream.recv(buffer, sizeof(buffer)) == -1); // Dispatch before EOF.
    std::string reply = "close reply";
    CHECK(stream.send(&reply[0], reply.size()) == reply.size());
    CHECK(stream.recv(buffer, sizeof(buffer)) == 0);
}

TEST_CASE("HTTP2 handles zero flow-control windows without losing bytes", "[http2]")
{
    auto peer = new Peer;
    peer->incoming.front() = frame(4, 0, 0, hex("000800000001000400000000"));
    ix::SocketHttp2 stream {std::unique_ptr<ix::Socket>(peer)};
    REQUIRE(stream.handshake(request, [] { return false; }).success);
    std::string body = "payload";
    CHECK(stream.send(&body[0], body.size()) == -1);
    CHECK(ix::Socket::isWaitNeeded());
    peer->incoming.push_back(frame(8, 0, 1, hex("00000007")));
    CHECK(stream.isReadyToWrite(0) == ix::PollResultType::ReadyForWrite);
    CHECK(stream.send(&body[0], body.size()) == body.size());
    CHECK(stream.isReadyToWrite(0) == ix::PollResultType::Timeout);
}

TEST_CASE("HTTP2 handshake cancellation is bounded without sleeping", "[http2]")
{
    auto peer = new Peer;
    peer->incoming.clear();
    ix::SocketHttp2 stream {std::unique_ptr<ix::Socket>(peer)};
    unsigned checks = 0;
    const auto result = stream.handshake(request, [&] { return ++checks > 30; });
    CHECK_FALSE(result.success);
    CHECK(result.errorStr.find("cancelled") != std::string::npos);
    CHECK(checks <= 32);
}

TEST_CASE("HTTP2 unsuccessful CONNECT preserves the HTTP status and headers", "[http2]")
{
    auto peer = new Peer;
    peer->incoming.back() =
        frame(1, 4, 1, ix::Hpack::encode({{":status", "403"}, {"x-error", "denied"}})) +
        frame(0, 1, 1, "error body");
    ix::SocketHttp2 stream {std::unique_ptr<ix::Socket>(peer)};
    const auto result = stream.handshake(request, [] { return false; });
    CHECK_FALSE(result.success);
    CHECK(result.http_status == 403);
    CHECK(result.headers.at("x-error") == "denied");
    CHECK(result.errorStr.find("403") != std::string::npos);
}

TEST_CASE("ALPN protocol offers are length bounded", "[http2][tls]")
{
    ix::SocketTLSOptions options;
    options.alpnProtocols = {"h2"};
    REQUIRE(options.isValid());
    options.alpnProtocols = {""};
    CHECK_FALSE(options.isValid());
    options.alpnProtocols = {std::string(256, 'x')};
    CHECK_FALSE(options.isValid());
}

TEST_CASE("HTTP2 enforces its receive window while an application is sending", "[http2]")
{
    auto peer = new Peer;
    peer->incoming.front() = frame(4, 0, 0, hex("000800000001000400000000"));
    ix::SocketHttp2 stream {std::unique_ptr<ix::Socket>(peer)};
    REQUIRE(stream.handshake(request, [] { return false; }).success);
    for (unsigned i = 0; i < 4; ++i)
    {
        peer->incoming.push_back(frame(0, 0, 1, std::string(16384, 'x')));
    }
    // The app is blocked on its send window, so incoming payload isn't consumed.
    // Credit is replenished only when recv() delivers bytes to the app.
    bool rejected = false;
    for (unsigned i = 0; i < 8; ++i)
    {
        char byte = 'x';
        if (stream.send(&byte, 1) < 0 && !ix::Socket::isWaitNeeded())
        {
            rejected = true;
            break;
        }
    }
    CHECK(rejected);
}

TEST_CASE("HTTP2 flushes partial control writes while its DATA window is blocked", "[http2]")
{
    auto peer = new Peer;
    peer->incoming.front() = frame(4, 0, 0, hex("000800000001000400000000"));
    ix::SocketHttp2 stream {std::unique_ptr<ix::Socket>(peer)};
    REQUIRE(stream.handshake(request, [] { return false; }).success);
    peer->sent.clear();
    peer->partial = true;
    peer->incoming.push_back(frame(0, 0, 1, "x"));
    char byte;
    REQUIRE(stream.recv(&byte, 1) == 1);
    const auto updates = frame(8, 0, 0, hex("00000001")) + frame(8, 0, 1, hex("00000001"));
    for (unsigned attempt = 0; attempt < 32 && peer->sent.size() < updates.size(); ++attempt)
    {
        stream.isReadyToWrite(0);
    }
    CHECK(peer->sent == updates);
    CHECK(stream.isReadyToWrite(0) == ix::PollResultType::Timeout);
}

TEST_CASE("HTTP2 exposes EOF while a send is blocked on flow control", "[http2]")
{
    auto peer = new Peer;
    peer->incoming.front() = frame(4, 0, 0, hex("000800000001000400000000"));
    ix::SocketHttp2 stream {std::unique_ptr<ix::Socket>(peer)};
    REQUIRE(stream.handshake(request, [] { return false; }).success);
    peer->incoming.push_back(frame(0, 1, 1, ""));
    stream.isReadyToWrite(0);
    CHECK(stream.isReadyToWrite(0) == ix::PollResultType::ReadyForRead);
    char byte;
    CHECK(stream.recv(&byte, 1) == 0);
}
