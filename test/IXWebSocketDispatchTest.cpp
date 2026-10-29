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
#include <ixwebsocket/IXWebSocketServer.h>
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
                    _sent =
                        _frames.empty() ||
                        rawSocket->writeBytes(
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

TEST_CASE("Poll errors close the transport after buffered frames are dispatched",
          "[websocket][close][poll]")
{
    const int buffered = GENERATE(0, 1, 2);
    CAPTURE(buffered);
    std::string frames;
    if (buffered != 0) frames = frame(0x81, "before");
    if (buffered == 2) frames += frame(0x88, std::string("\x03\xe8", 2) + "done");
    DispatchTestServer server(frames);
    Transport client;
    server.bufferFrames(client);
    std::vector<std::string> events;
    client.setOnCloseCallback(
        [&](uint16_t code, const std::string& reason, size_t, bool remote)
        {
            CHECK(code == (buffered == 2 ? 1000 : 1006));
            CHECK(reason ==
                  (buffered == 2 ? "done" : ix::WebSocketCloseConstants::kAbnormalCloseMessage));
            CHECK(remote == (buffered == 2));
            events.push_back("close");
        });
    auto onMessage = [&](const std::string& message, size_t, bool, Transport::MessageKind)
    { events.push_back(message); };

    // EOF closed the underlying descriptor without dispatching its buffered frames.
    // Polling it again must report failure, so run() cannot spin in the OPEN state.
    const auto result = client.poll();
    CHECK(result == Transport::PollResult::AbnormalClose);
    client.dispatch(result, onMessage);
    CHECK(client.getReadyState() == Transport::ReadyState::CLOSED);
    const std::vector<std::string> expected = buffered == 0
                                                  ? std::vector<std::string> {"close"}
                                                  : std::vector<std::string> {"before", "close"};
    CHECK(events == expected);
    client.dispatch(client.poll(), onMessage);
    CHECK(events == expected);
}

TEST_CASE("Incomplete or invalid frame lengths cannot prevent failure cleanup",
          "[websocket][close][poll]")
{
    const auto partial = GENERATE(std::string("\x81", 1),
                                  std::string("\x81\x7e\x00", 3),
                                  std::string("\x81\x05OL", 4),
                                  std::string("\x81\x7f\xff\xff\xff\xff\xff\xff\xff\xff", 10));
    DispatchTestServer server(frame(0x81, "before") + partial);
    Transport client;
    server.bufferFrames(client);
    std::vector<std::string> events;
    client.setOnCloseCallback(
        [&](uint16_t code, const std::string&, size_t, bool)
        {
            CHECK(code == 1006);
            events.push_back("close");
        });
    client.dispatch(client.poll(),
                    [&](const std::string& message, size_t, bool, Transport::MessageKind)
                    { events.push_back(message); });
    CHECK(client.getReadyState() == Transport::ReadyState::CLOSED);
    CHECK(events == std::vector<std::string> {"before", "close"});
}

TEST_CASE("Close-request interrupts complete a local close exactly once",
          "[websocket][close][poll]")
{
    ix::WebSocketServer server(ix::getFreePort(), "127.0.0.1");
    server.setOnClientMessageCallback([](std::shared_ptr<ix::ConnectionState>,
                                         ix::WebSocket&,
                                         const ix::WebSocketMessagePtr&) { });
    REQUIRE(server.listen().first);
    server.start();
    Transport client;
    client.configure(ix::WebSocketPerMessageDeflateOptions(false), {}, false, 0);
    REQUIRE(client.connectToUrl("ws://127.0.0.1:" + std::to_string(server.getPort()) + "/", {}, 2)
                .success);
    unsigned closes = 0;
    client.setOnCloseCallback(
        [&](uint16_t code, const std::string& reason, size_t, bool remote)
        {
            CHECK(code == 4000);
            CHECK(reason == "done");
            CHECK_FALSE(remote);
            ++closes;
        });
    auto onMessage = [](const std::string&, size_t, bool, Transport::MessageKind)
    { FAIL("Closing must not deliver messages"); };

    client.close(4000, "done");
    // The first close queues a SendRequest, which must still be handled normally.
    REQUIRE(client.poll() == Transport::PollResult::Succeeded);
    REQUIRE(client.getReadyState() == Transport::ReadyState::CLOSING);
    // Closing again interrupts the poll with CloseRequest instead of sending another frame.
    client.close(4001, "ignored");
    const auto result = client.poll();
    CHECK(result == Transport::PollResult::AbnormalClose);
    client.dispatch(result, onMessage);
    CHECK(client.getReadyState() == Transport::ReadyState::CLOSED);
    CHECK(closes == 1);
    client.dispatch(client.poll(), onMessage);
    CHECK(closes == 1);
}

TEST_CASE("Parsing errors after socket failure retain their protocol close code",
          "[websocket][close][poll]")
{
    const auto invalid = GENERATE(std::string("\xa1\x00", 2),
                                  std::string("\x09\x00", 2),
                                  std::string("\x89\x7e\x00\x7e", 4) + std::string(126, 'x'));
    DispatchTestServer server(invalid);
    Transport client;
    server.bufferFrames(client);
    unsigned closes = 0;
    client.setOnCloseCallback(
        [&](uint16_t code, const std::string&, size_t, bool)
        {
            CHECK(code == 1002);
            ++closes;
        });
    client.dispatch(client.poll(),
                    [](const std::string&, size_t, bool, Transport::MessageKind)
                    { FAIL("Invalid frames must not deliver messages"); });
    CHECK(client.getReadyState() == Transport::ReadyState::CLOSED);
    CHECK(closes == 1);
}
