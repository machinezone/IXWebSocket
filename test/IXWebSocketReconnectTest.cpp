/*
 *  IXWebSocketReconnectTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */

#include <atomic>
#include <catch_amalgamated.hpp>
#include <chrono>
#include <condition_variable>
#include <future>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXSocketServer.h>
#include <ixwebsocket/IXWebSocket.h>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;
    using Transport = ix::WebSocketTransport;

    class ReconnectTestServer final : public ix::SocketServer
    {
    public:
        using Script = std::function<bool(ix::Socket&, unsigned, const ix::CancellationRequest&)>;

        explicit ReconnectTestServer(const Script& script)
            : SocketServer(ix::getFreePort(), "127.0.0.1")
            , _script(script)
        {
        }

        ~ReconnectTestServer() override
        {
            stop();
        }

        void stop() override
        {
            _stopping = true;
            SocketServer::stop();
        }

        std::string url()
        {
            return "ws://127.0.0.1:" + std::to_string(getPort()) + "/";
        }

        unsigned errors() const
        {
            return _errors;
        }

    private:
        void handleConnection(std::unique_ptr<ix::Socket> socket,
                              std::shared_ptr<ix::ConnectionState> state) override
        {
            unsigned index = _connections++;
            {
                auto* rawSocket = socket.get();
                Transport transport;
                auto result = transport.connectToSocket(std::move(socket), 2, false);
                const auto deadline = Clock::now() + std::chrono::seconds(5);
                auto cancelled = [this, deadline] { return _stopping || Clock::now() >= deadline; };
                if (!result.success || !_script(*rawSocket, index, cancelled)) ++_errors;
            }
            state->setTerminated();
        }

        size_t getConnectedClientsCount() override
        {
            return 0;
        }

        Script _script;
        std::atomic<bool> _stopping {false};
        std::atomic<unsigned> _connections {0};
        std::atomic<unsigned> _errors {0};
    };

    void configureReconnectTestTransport(Transport& transport)
    {
        // Poll without blocking so that test deadlines also cover incomplete frames.
        transport.configure(ix::WebSocketPerMessageDeflateOptions(false), {}, false, 0);
    }
} // namespace

TEST_CASE("Automatic reconnect discards old message fragments", "[websocket][reconnect]")
{
    ReconnectTestServer server(
        [](ix::Socket& socket, unsigned index, const ix::CancellationRequest& cancelled)
        {
            if (index == 0) return socket.writeBytes(std::string("\x01\x03OLD", 5), cancelled);
            if (!socket.writeBytes(std::string("\x81\x03NEW", 5), cancelled)) return false;
            char byte;
            socket.readByte(&byte, cancelled); // Keep this connection open until the client stops.
            return true;
        });
    REQUIRE(server.listen().first);
    server.start();

    std::mutex mutex;
    std::condition_variable changed;
    std::vector<std::string> messages;
    unsigned fragments = 0;
    ix::WebSocket client;
    client.setUrl(server.url());
    client.disablePerMessageDeflate();
    client.setMinWaitBetweenReconnectionRetries(10);
    client.setMaxWaitBetweenReconnectionRetries(10);
    client.setOnMessageCallback(
        [&](const ix::WebSocketMessagePtr& message)
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (message->type == ix::WebSocketMessageType::Fragment) ++fragments;
            if (message->type == ix::WebSocketMessageType::Message)
            {
                messages.push_back(message->str);
                changed.notify_all();
            }
        });
    client.start();
    bool received;
    {
        std::unique_lock<std::mutex> lock(mutex);
        received =
            changed.wait_for(lock, std::chrono::seconds(5), [&] { return !messages.empty(); });
    }
    client.stop();
    server.stop();

    REQUIRE(received);
    REQUIRE(messages.size() == 1);
    CHECK(messages.front() == "NEW");
    CHECK(fragments == 1);
    CHECK(server.errors() == 0);
}

TEST_CASE("Reconnect discards an incomplete received frame", "[websocket][reconnect]")
{
    std::atomic<bool> releaseFirst {false};
    ReconnectTestServer server(
        [&](ix::Socket& socket, unsigned index, const ix::CancellationRequest& cancelled)
        {
            if (index == 0)
            {
                if (!socket.writeBytes(std::string("\x81\x05OL", 4), cancelled)) return false;
                while (!releaseFirst && !cancelled())
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return true;
            }
            return socket.writeBytes(std::string("\x81\x03NEW", 5), cancelled);
        });
    REQUIRE(server.listen().first);
    server.start();

    Transport client;
    configureReconnectTestTransport(client);
    REQUIRE(client.connectToUrl(server.url(), {}, 2).success);
    std::vector<std::string> messages;
    auto onMessage = [&](const std::string& message, size_t, bool, Transport::MessageKind kind)
    {
        if (kind == Transport::MessageKind::MSG_TEXT) messages.push_back(message);
    };
    const auto deadline = Clock::now() + std::chrono::milliseconds(100);
    while (Clock::now() < deadline)
    {
        client.dispatch(client.poll(), onMessage);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(messages.empty());
    CHECK(client.getReadyState() == Transport::ReadyState::OPEN);

    // A send-side failure can close the socket before dispatch consumes its receive buffer.
    client.closeSocket();
    client.setReadyState(Transport::ReadyState::CLOSED);
    releaseFirst = true;
    REQUIRE(client.connectToUrl(server.url(), {}, 2).success);
    const auto receiveDeadline = Clock::now() + std::chrono::seconds(3);
    while (messages.empty() && Clock::now() < receiveDeadline &&
           client.getReadyState() == Transport::ReadyState::OPEN)
    {
        client.dispatch(client.poll(), onMessage);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    client.closeSocket();
    server.stop();
    REQUIRE(messages.size() == 1);
    CHECK(messages.front() == "NEW");
    CHECK(server.errors() == 0);
}

TEST_CASE("Reconnect discards queued output", "[websocket][reconnect]")
{
    const bool invalidUrl = GENERATE(false, true);
    CAPTURE(invalidUrl);
    std::atomic<bool> releaseFirst {false};
    std::promise<std::string> received;
    auto wire = received.get_future();
    ReconnectTestServer server(
        [&](ix::Socket& socket, unsigned index, const ix::CancellationRequest& cancelled)
        {
            if (index == 0)
            {
                // Deliberately do not read, so the client's nonblocking send queues output.
                while (!releaseFirst && !cancelled())
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return true;
            }
            if (index != 1) return false;
            auto result = socket.readBytes(9, nullptr, nullptr, cancelled);
            received.set_value(result.second);
            return result.first;
        });
    REQUIRE(server.listen().first);
    server.start();

    Transport client;
    configureReconnectTestTransport(client);
    REQUIRE(client.connectToUrl(server.url(), {}, 2).success);
    const std::string payload(4 * 1024 * 1024, 'x');
    for (int i = 0; i < 4 && client.bufferedAmount() == 0; ++i)
        REQUIRE(client.sendBinary(payload, nullptr).success);
    REQUIRE(client.bufferedAmount() > 0);
    client.closeSocket();
    client.setReadyState(Transport::ReadyState::CLOSED);
    releaseFirst = true;

    auto result = client.connectToUrl(invalidUrl ? "invalid-url" : server.url(), {}, 2);
    CHECK(result.success == !invalidUrl);
    CHECK(client.bufferedAmount() == 0);
    if (!invalidUrl)
    {
        REQUIRE(result.success);
        CHECK(client.sendText(std::string("NEW"), nullptr).success);
        REQUIRE(wire.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
        auto frame = wire.get();
        REQUIRE(frame.size() == 9);
        CHECK(static_cast<unsigned char>(frame[0]) == 0x81);
        CHECK(static_cast<unsigned char>(frame[1]) == 0x83);
        std::string message;
        for (size_t i = 0; i < 3; ++i)
            message += frame[6 + i] ^ frame[2 + i];
        CHECK(message == "NEW");
    }
    client.closeSocket();
    server.stop();
    CHECK(server.errors() == 0);
}

TEST_CASE("Reconnect waits for an interrupted send", "[websocket][reconnect]")
{
    std::promise<std::string> received;
    auto wire = received.get_future();
    ReconnectTestServer server(
        [&](ix::Socket& socket, unsigned index, const ix::CancellationRequest& cancelled)
        {
            if (index == 0) return socket.readBytes(32768 + 8, nullptr, nullptr, cancelled).first;
            if (index != 1) return false;
            auto result = socket.readBytes(9, nullptr, nullptr, cancelled);
            received.set_value(result.second);
            char byte;
            socket.readByte(&byte, cancelled);
            return result.first;
        });
    REQUIRE(server.listen().first);
    server.start();

    std::mutex mutex;
    std::condition_variable changed;
    unsigned opens = 0;
    bool closed = false;
    bool paused = false;
    bool release = false;
    ix::WebSocket client;
    client.setUrl(server.url());
    client.disablePerMessageDeflate();
    client.setOnMessageCallback(
        [&](const ix::WebSocketMessagePtr& message)
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (message->type == ix::WebSocketMessageType::Open) ++opens;
            if (message->type == ix::WebSocketMessageType::Close) closed = true;
            changed.notify_all();
        });
    client.start();
    bool connected;
    {
        std::unique_lock<std::mutex> lock(mutex);
        connected = changed.wait_for(lock, std::chrono::seconds(5), [&] { return opens == 1; });
    }
    if (!connected)
    {
        client.stop();
        FAIL("Initial connection failed");
    }

    bool sendSucceeded = true;
    std::thread sender(
        [&]
        {
            sendSucceeded = client
                                .sendBinary(std::string(65536, 'x'),
                                            [&](int step, int)
                                            {
                                                if (step == 0)
                                                {
                                                    std::unique_lock<std::mutex> lock(mutex);
                                                    paused = true;
                                                    changed.notify_all();
                                                    changed.wait_for(lock,
                                                                     std::chrono::seconds(5),
                                                                     [&] { return release; });
                                                }
                                                return true;
                                            })
                                .success;
        });
    bool interrupted;
    bool reconnectedWhilePaused;
    {
        std::unique_lock<std::mutex> lock(mutex);
        interrupted =
            changed.wait_for(lock, std::chrono::seconds(3), [&] { return closed && paused; });
        reconnectedWhilePaused =
            changed.wait_for(lock, std::chrono::milliseconds(100), [&] { return opens > 1; });
        release = true;
        changed.notify_all();
    }
    sender.join();
    bool reconnected;
    {
        std::unique_lock<std::mutex> lock(mutex);
        reconnected = changed.wait_for(lock, std::chrono::seconds(3), [&] { return opens >= 2; });
    }
    bool freshSent = reconnected && client.sendText("NEW").success;
    bool receivedFresh = wire.wait_for(std::chrono::seconds(3)) == std::future_status::ready;
    client.stop();
    server.stop();

    CHECK(interrupted);
    CHECK_FALSE(reconnectedWhilePaused);
    CHECK_FALSE(sendSucceeded);
    CHECK(reconnected);
    CHECK(freshSent);
    REQUIRE(receivedFresh);
    auto frame = wire.get();
    REQUIRE(frame.size() == 9);
    CHECK(static_cast<unsigned char>(frame[0]) == 0x81);
    CHECK(static_cast<unsigned char>(frame[1]) == 0x83);
    std::string message;
    for (size_t i = 0; i < 3; ++i)
        message += frame[6 + i] ^ frame[2 + i];
    CHECK(message == "NEW");
    CHECK(server.errors() == 0);
}
