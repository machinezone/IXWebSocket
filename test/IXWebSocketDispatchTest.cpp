/*
 *  IXWebSocketDispatchTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */

#include <atomic>
#include <catch_amalgamated.hpp>
#include <chrono>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXSocketServer.h>
#include <ixwebsocket/IXWebSocketTransport.h>
#include <thread>
#include <vector>

namespace
{
    using Transport = ix::WebSocketTransport;

    std::string frame(unsigned char opcode, const std::string& payload)
    {
        REQUIRE(payload.size() <= 125);
        return std::string {static_cast<char>(opcode), static_cast<char>(payload.size())} + payload;
    }

    std::string lateFrames()
    {
        return frame(0x81, "late text") + frame(0x82, "late binary") + frame(0x89, "ping") +
               frame(0x8a, "pong") + frame(0x01, "fragment") + frame(0x80, "end");
    }

    class DispatchTestServer final : public ix::SocketServer
    {
    public:
        explicit DispatchTestServer(const std::string& frames)
            : SocketServer(ix::getFreePort(), "127.0.0.1")
            , _frames(frames)
        {
        }

        ~DispatchTestServer() override
        {
            stop();
        }

        void bufferFrames(Transport& client)
        {
            REQUIRE(listen().first);
            start();
            client.configure(ix::WebSocketPerMessageDeflateOptions(false), {}, false, 0);
            REQUIRE(client.connectToUrl("ws://127.0.0.1:" + std::to_string(getPort()) + "/", {}, 2)
                        .success);

            // Wait for EOF without dispatching. All frames are then in the receive buffer,
            // regardless of how TCP split the writes. The transport stays OPEN until dispatch.
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
            auto result = Transport::PollResult::Succeeded;
            while (result == Transport::PollResult::Succeeded &&
                   std::chrono::steady_clock::now() < deadline)
            {
                result = client.poll();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            REQUIRE(result == Transport::PollResult::AbnormalClose);
            REQUIRE(_sent);
            REQUIRE(client.getReadyState() == Transport::ReadyState::OPEN);
        }

    private:
        void handleConnection(std::unique_ptr<ix::Socket> socket,
                              std::shared_ptr<ix::ConnectionState> state) override
        {
            {
                auto* rawSocket = socket.get();
                Transport server;
                if (server.connectToSocket(std::move(socket), 2, false).success)
                {
                    const auto deadline =
                        std::chrono::steady_clock::now() + std::chrono::seconds(3);
                    _sent = rawSocket->writeBytes(
                        _frames,
                        [deadline] { return std::chrono::steady_clock::now() >= deadline; });
                }
            }
            state->setTerminated();
        }

        size_t getConnectedClientsCount() override
        {
            return 0;
        }

        const std::string _frames;
        std::atomic<bool> _sent {false};
    };
} // namespace

TEST_CASE("Dispatch stops at a peer close frame", "[websocket][close][dispatch]")
{
    const auto pollResult =
        GENERATE(Transport::PollResult::Succeeded, Transport::PollResult::AbnormalClose);
    DispatchTestServer server(frame(0x81, "before") + frame(0x88, std::string("\x03\xe8", 2)) +
                              lateFrames());
    Transport client;
    server.bufferFrames(client);
    std::vector<std::string> events;
    client.setOnCloseCallback(
        [&](uint16_t code, const std::string& reason, size_t, bool remote)
        {
            CHECK(code == 1000);
            CHECK(reason.empty());
            CHECK(remote);
            events.push_back("close");
        });
    auto onMessage = [&](const std::string& message, size_t, bool, Transport::MessageKind)
    { events.push_back(message); };
    client.dispatch(pollResult, onMessage);
    // A later dispatch must not expose frames left after the Close frame either.
    client.dispatch(Transport::PollResult::Succeeded, onMessage);
    CHECK(client.getReadyState() == Transport::ReadyState::CLOSED);
    CHECK(events == std::vector<std::string> {"before", "close"});
}

TEST_CASE("Closing in a callback suppresses remaining buffered callbacks",
          "[websocket][close][dispatch]")
{
    const unsigned char opcode = GENERATE(0x81, 0x82, 0x89, 0x8a, 0x01);
    CAPTURE(opcode);
    const std::string closePayload = std::string("\x03\xe8", 2) + "done";
    const std::string continuation = opcode == 0x01 ? frame(0x80, "end") : "";
    DispatchTestServer server(frame(opcode, "first") + continuation + lateFrames() +
                              frame(0x88, closePayload));
    Transport client;
    server.bufferFrames(client);
    unsigned callbacks = 0;
    unsigned closes = 0;
    client.setOnCloseCallback(
        [&](uint16_t code, const std::string& reason, size_t, bool remote)
        {
            CHECK(code == 1000);
            CHECK(reason == "done");
            CHECK_FALSE(remote);
            ++closes;
        });
    client.dispatch(Transport::PollResult::Succeeded,
                    [&](const std::string&, size_t, bool, Transport::MessageKind)
                    {
                        ++callbacks;
                        client.close(1000, "done");
                    });
    CHECK(callbacks == 1);
    CHECK(closes == 1);
    CHECK(client.getReadyState() == Transport::ReadyState::CLOSED);
}

TEST_CASE("Dispatch suppresses callbacks when already closing or closed",
          "[websocket][close][dispatch]")
{
    const bool closed = GENERATE(false, true);
    CAPTURE(closed);
    DispatchTestServer server(lateFrames() + frame(0x88, std::string("\x03\xe8", 2) + "done"));
    Transport client;
    server.bufferFrames(client);
    unsigned callbacks = 0;
    unsigned closes = 0;
    client.setOnCloseCallback([&](uint16_t, const std::string&, size_t, bool) { ++closes; });
    client.close(1000, "done");
    if (closed) client.setReadyState(Transport::ReadyState::CLOSED);
    client.dispatch(Transport::PollResult::AbnormalClose,
                    [&](const std::string&, size_t, bool, Transport::MessageKind) { ++callbacks; });
    CHECK(callbacks == 0);
    CHECK(closes == 1);
    CHECK(client.getReadyState() == Transport::ReadyState::CLOSED);
}

TEST_CASE("Invalid text closes without delivering later buffered frames",
          "[websocket][close][dispatch]")
{
    DispatchTestServer server(frame(0x81, std::string("\xff", 1)) + lateFrames());
    Transport client;
    server.bufferFrames(client);
    unsigned callbacks = 0;
    unsigned closes = 0;
    client.setOnCloseCallback(
        [&](uint16_t code, const std::string&, size_t, bool)
        {
            CHECK(code == 1007);
            ++closes;
        });
    client.dispatch(Transport::PollResult::AbnormalClose,
                    [&](const std::string&, size_t, bool, Transport::MessageKind) { ++callbacks; });
    CHECK(callbacks == 0);
    CHECK(closes == 1);
    CHECK(client.getReadyState() == Transport::ReadyState::CLOSED);
}
