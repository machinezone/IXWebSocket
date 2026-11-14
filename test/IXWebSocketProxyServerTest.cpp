/*
 *  IXWebSocketProxyServerTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <atomic>
#include <catch_amalgamated.hpp>
#include <chrono>
#include <condition_variable>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXWebSocketProxyServerPrivate.h>
#include <mutex>
#include <thread>

namespace
{
    using Clock = std::chrono::steady_clock;

    struct Counts
    {
        std::atomic<unsigned> created {0};
        std::atomic<unsigned> destroyed {0};
    };

    class CountedState final : public ix::detail::ProxyConnectionState
    {
    public:
        explicit CountedState(Counts& counts)
            : _counts(counts)
        {
            ++_counts.created;
        }

        ~CountedState() override
        {
            // Count completion only after the owned upstream thread has been joined.
            webSocket().stop();
            ++_counts.destroyed;
        }

    private:
        Counts& _counts;
    };

    bool waitFor(const std::atomic<unsigned>& value, unsigned expected)
    {
        const auto deadline = Clock::now() + std::chrono::seconds(2);
        while (value != expected && Clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return value == expected;
    }

    std::string url(ix::SocketServer& server)
    {
        return "ws://127.0.0.1:" + std::to_string(server.getPort());
    }

    void configureProxy(ix::WebSocketServer& proxy, const std::string& remote, Counts& counts)
    {
        ix::detail::configureWebSocketProxyServer(proxy, remote, {});
        proxy.setConnectionStateFactory([&counts]
                                        { return std::make_shared<CountedState>(counts); });
        REQUIRE(proxy.listen().first);
        proxy.start();
    }

    void startOrigin(ix::WebSocketServer& origin)
    {
        origin.setOnClientMessageCallback(
            [](std::shared_ptr<ix::ConnectionState>,
               ix::WebSocket& socket,
               const ix::WebSocketMessagePtr& message)
            {
                if (message->type != ix::WebSocketMessageType::Message) return;
                if (message->str == "upstream-close")
                    socket.close(4002, "upstream done");
                else
                    socket.send(message->str, message->binary);
            });
        REQUIRE(origin.listen().first);
        origin.start();
    }

    struct Client
    {
        std::mutex mutex;
        std::condition_variable changed;
        bool opened = false;
        bool failed = false;
        bool closed = false;
        bool binary = false;
        std::string received;
        uint16_t code = 0;
        std::string reason;
        ix::WebSocket socket;

        explicit Client(const std::string& remote)
        {
            socket.disableAutomaticReconnection();
            socket.setUrl(remote);
            socket.setOnMessageCallback(
                [this](const ix::WebSocketMessagePtr& message)
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    if (message->type == ix::WebSocketMessageType::Open) opened = true;
                    if (message->type == ix::WebSocketMessageType::Error) failed = true;
                    if (message->type == ix::WebSocketMessageType::Message)
                    {
                        received = message->str;
                        binary = message->binary;
                    }
                    if (message->type == ix::WebSocketMessageType::Close)
                    {
                        closed = true;
                        code = message->closeInfo.code;
                        reason = message->closeInfo.reason;
                    }
                    changed.notify_all();
                });
            socket.start();
        }

        ~Client()
        {
            socket.stop();
        }

        bool wait(const std::function<bool()>& ready)
        {
            std::unique_lock<std::mutex> lock(mutex);
            return changed.wait_for(lock, std::chrono::seconds(2), ready);
        }
    };

    class UnavailableOrigin final : public ix::SocketServer
    {
    public:
        explicit UnavailableOrigin(bool idle)
            : SocketServer(ix::getFreePort(), "127.0.0.1")
            , _idle(idle)
        {
        }

        ~UnavailableOrigin() override
        {
            stop();
        }

        void stop() override
        {
            _stopping = true;
            SocketServer::stop();
        }

        std::atomic<unsigned> accepted {0};

    private:
        void handleConnection(std::unique_ptr<ix::Socket>,
                              std::shared_ptr<ix::ConnectionState> state) override
        {
            ++accepted;
            while (_idle && !_stopping)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            state->setTerminated();
        }

        size_t getConnectedClientsCount() override
        {
            return 0;
        }

        bool _idle;
        std::atomic<bool> _stopping {false};
    };
} // namespace

TEST_CASE("Proxy sessions are released after downstream disconnect", "[proxy][lifetime]")
{
    Counts counts;
    ix::WebSocketServer origin(ix::getFreePort(), "127.0.0.1");
    startOrigin(origin);
    ix::WebSocketServer proxy(ix::getFreePort(), "127.0.0.1");
    configureProxy(proxy, url(origin), counts);
    for (unsigned i = 0; i != 5; ++i)
    {
        Client client(url(proxy) + "/echo");
        REQUIRE(client.wait([&] { return client.opened || client.failed; }));
        REQUIRE(client.opened);
        const bool binary = i % 2 != 0;
        REQUIRE(client.socket.send("echo", binary).success);
        REQUIRE(client.wait([&] { return client.received == "echo" && client.binary == binary; }));
        client.socket.stop(4001, "downstream done");
        CHECK(waitFor(counts.destroyed, i + 1));
    }
    proxy.stop();
    origin.stop();
    CHECK(counts.created == 5);
    CHECK(counts.destroyed == counts.created);
}

TEST_CASE("Upstream closure releases the proxy session", "[proxy][lifetime]")
{
    Counts counts;
    ix::WebSocketServer origin(ix::getFreePort(), "127.0.0.1");
    startOrigin(origin);
    ix::WebSocketServer proxy(ix::getFreePort(), "127.0.0.1");
    configureProxy(proxy, url(origin), counts);
    Client client(url(proxy));
    REQUIRE(client.wait([&] { return client.opened || client.failed; }));
    REQUIRE(client.opened);
    REQUIRE(client.socket.send("upstream-close").success);
    REQUIRE(client.wait([&] { return client.closed; }));
    client.socket.stop();
    CHECK(client.code == 4002);
    CHECK(client.reason == "upstream done");
    CHECK(waitFor(counts.destroyed, 1));
    proxy.stop();
    origin.stop();
    CHECK(counts.destroyed == counts.created);
}

TEST_CASE("Stopping a proxy releases live sessions", "[proxy][lifetime]")
{
    Counts counts;
    ix::WebSocketServer origin(ix::getFreePort(), "127.0.0.1");
    startOrigin(origin);
    ix::WebSocketServer proxy(ix::getFreePort(), "127.0.0.1");
    configureProxy(proxy, url(origin), counts);
    Client client(url(proxy));
    REQUIRE(client.wait([&] { return client.opened || client.failed; }));
    REQUIRE(client.opened);
    proxy.stop();
    REQUIRE(client.wait([&] { return client.closed; }));
    client.socket.stop();
    origin.stop();
    CHECK(counts.created == 1);
    CHECK(counts.destroyed == counts.created);
}

TEST_CASE("Failed and pending upstream handshakes can be cleaned up", "[proxy][lifetime]")
{
    const bool idle = GENERATE(false, true);
    Counts counts;
    UnavailableOrigin origin(idle);
    REQUIRE(origin.listen().first);
    origin.start();
    ix::WebSocketServer proxy(ix::getFreePort(), "127.0.0.1");
    configureProxy(proxy, url(origin), counts);
    Client client(url(proxy));
    REQUIRE(client.wait([&] { return client.opened || client.failed; }));
    REQUIRE(client.opened);
    REQUIRE(waitFor(origin.accepted, 1));
    if (!idle)
    {
        REQUIRE(client.wait([&] { return client.closed; }));
        client.socket.stop();
        CHECK(client.code == 1011);
    }
    const auto start = Clock::now();
    proxy.stop();
    CHECK(Clock::now() - start < std::chrono::seconds(2));
    client.socket.stop();
    origin.stop();
    CHECK(counts.created == 1);
    CHECK(counts.destroyed == counts.created);
}
