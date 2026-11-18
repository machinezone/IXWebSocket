/*
 *  IXWebSocketProxyServerTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <algorithm>
#include <atomic>
#include <catch_amalgamated.hpp>
#include <chrono>
#include <condition_variable>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXWebSocketProxyServer.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;

    struct States
    {
        std::mutex mutex;
        std::vector<std::weak_ptr<ix::ConnectionState>> connections;

        void observe(const std::shared_ptr<ix::ConnectionState>& state)
        {
            std::lock_guard<std::mutex> lock(mutex);
            connections.push_back(state);
        }

        size_t created()
        {
            std::lock_guard<std::mutex> lock(mutex);
            return connections.size();
        }

        size_t released()
        {
            std::lock_guard<std::mutex> lock(mutex);
            return static_cast<size_t>(std::count_if(
                connections.begin(),
                connections.end(),
                [](const std::weak_ptr<ix::ConnectionState>& state) { return state.expired(); }));
        }
    };

    class ObservedProxy final : public ix::WebSocketServer
    {
    public:
        explicit ObservedProxy(States& states)
            : WebSocketServer(ix::getFreePort(), "127.0.0.1")
            , _states(states)
        {
        }

        ~ObservedProxy() override
        {
            stop();
        }

    private:
        void handleConnection(std::unique_ptr<ix::Socket> socket,
                              std::shared_ptr<ix::ConnectionState> state) override
        {
            _states.observe(state);
            handleUpgrade(std::move(socket), state);
            state->setTerminated();
        }

        States& _states;
    };

    bool waitForRelease(States& states, unsigned expected)
    {
        const auto deadline = Clock::now() + std::chrono::seconds(2);
        while (states.released() != expected && Clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return states.released() == expected;
    }

    bool waitFor(const std::atomic<unsigned>& value, unsigned expected)
    {
        const auto deadline = Clock::now() + std::chrono::seconds(2);
        while (value != expected && Clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return value == expected;
    }

    std::string url(ix::SocketServer& server)
    {
        return "ws://127.0.0.1:" + std::to_string(server.getPort());
    }

    void configureProxy(ix::WebSocketServer& proxy, const std::string& remote)
    {
        ix::configureWebSocketProxyServer(proxy, remote, {});
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
                if (message->type != ix::WebSocketMessageType::Message)
                {
                    return;
                }
                if (message->str == "upstream-close")
                {
                    socket.close(4002, "upstream done");
                }
                else
                {
                    socket.send(message->str, message->binary);
                }
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
                    if (message->type == ix::WebSocketMessageType::Open)
                    {
                        opened = true;
                    }
                    if (message->type == ix::WebSocketMessageType::Error)
                    {
                        failed = true;
                    }
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
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
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
    States states;
    ix::WebSocketServer origin(ix::getFreePort(), "127.0.0.1");
    startOrigin(origin);
    ObservedProxy proxy(states);
    configureProxy(proxy, url(origin));
    for (unsigned i = 0; i != 5; ++i)
    {
        Client client(url(proxy) + "/echo");
        REQUIRE(client.wait([&] { return client.opened || client.failed; }));
        REQUIRE(client.opened);
        const bool binary = i % 2 != 0;
        REQUIRE(client.socket.send("echo", binary).success);
        REQUIRE(client.wait([&] { return client.received == "echo" && client.binary == binary; }));
        client.socket.stop(4001, "downstream done");
        CHECK(waitForRelease(states, i + 1));
    }
    proxy.stop();
    origin.stop();
    CHECK(states.created() == 5);
    CHECK(states.released() == states.created());
}

TEST_CASE("Upstream closure releases the proxy session", "[proxy][lifetime]")
{
    States states;
    ix::WebSocketServer origin(ix::getFreePort(), "127.0.0.1");
    startOrigin(origin);
    ObservedProxy proxy(states);
    configureProxy(proxy, url(origin));
    Client client(url(proxy));
    REQUIRE(client.wait([&] { return client.opened || client.failed; }));
    REQUIRE(client.opened);
    REQUIRE(client.socket.send("upstream-close").success);
    REQUIRE(client.wait([&] { return client.closed; }));
    client.socket.stop();
    CHECK(client.code == 4002);
    CHECK(client.reason == "upstream done");
    CHECK(waitForRelease(states, 1));
    proxy.stop();
    origin.stop();
    CHECK(states.released() == states.created());
}

TEST_CASE("Stopping a proxy releases live sessions", "[proxy][lifetime]")
{
    States states;
    ix::WebSocketServer origin(ix::getFreePort(), "127.0.0.1");
    startOrigin(origin);
    ObservedProxy proxy(states);
    configureProxy(proxy, url(origin));
    Client client(url(proxy));
    REQUIRE(client.wait([&] { return client.opened || client.failed; }));
    REQUIRE(client.opened);
    proxy.stop();
    REQUIRE(client.wait([&] { return client.closed; }));
    client.socket.stop();
    origin.stop();
    CHECK(states.created() == 1);
    CHECK(states.released() == states.created());
}

TEST_CASE("Failed and pending upstream handshakes can be cleaned up", "[proxy][lifetime]")
{
    const bool idle = GENERATE(false, true);
    States states;
    UnavailableOrigin origin(idle);
    REQUIRE(origin.listen().first);
    origin.start();
    ObservedProxy proxy(states);
    configureProxy(proxy, url(origin));
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
    CHECK(states.created() == 1);
    CHECK(states.released() == states.created());
}
