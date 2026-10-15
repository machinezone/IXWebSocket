/*
 *  IXSocketTLSServerTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */

#include <atomic>
#include <catch_amalgamated.hpp>
#include <chrono>
#include <condition_variable>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXHttpServer.h>
#include <mutex>
#include <thread>

namespace
{
    using Clock = std::chrono::steady_clock;

    std::string tlsServerCredential(const std::string& name)
    {
        return std::string(IX_TLS_SERVER_TEST_CERTS) + "/" + name;
    }

    class TLSAcceptTestServer
    {
    public:
        explicit TLSAcceptTestServer(size_t maxConnections = 4)
            : server(ix::getFreePort(), "127.0.0.1", 5, maxConnections)
        {
            ix::SocketTLSOptions options;
            options.tls = true;
            options.certFile = tlsServerCredential("server.pem");
            options.keyFile = tlsServerCredential("server.key");
            options.caFile = "NONE";
            server.setTLSOptions(options);
            server.setLogCallback(
                [this](ix::LogLevel, const std::string& message)
                {
                    if (message.find("TLS handshake timed out") != std::string::npos) ++timeouts;
                });
            server.setConnectionStateFactory(
                [this]
                {
                    {
                        std::lock_guard<std::mutex> lock(_mutex);
                        ++_accepted;
                    }
                    _changed.notify_all();
                    return ix::ConnectionState::createConnectionState();
                });
            server.setOnConnectionCallback(
                [](ix::HttpRequestPtr, std::shared_ptr<ix::ConnectionState>)
                {
                    return std::make_shared<ix::HttpResponse>(
                        200, "OK", ix::HttpErrorCode::Ok, ix::WebSocketHttpHeaders(), "ok");
                });
        }

        ~TLSAcceptTestServer()
        {
            server.stop();
        }

        void start()
        {
            auto result = server.listen();
            INFO(result.second);
            REQUIRE(result.first);
            server.start();
        }

        bool waitForAccepted(unsigned count)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            return _changed.wait_for(
                lock, std::chrono::seconds(3), [this, count] { return _accepted >= count; });
        }

        void connectIdlePeer(ix::Socket& peer)
        {
            std::string error;
            REQUIRE(peer.init(error));
            const auto deadline = Clock::now() + std::chrono::seconds(2);
            bool connected = peer.connect("127.0.0.1",
                                          server.getPort(),
                                          error,
                                          [deadline] { return Clock::now() >= deadline; });
            INFO(error);
            REQUIRE(connected);
        }

        ix::HttpResponsePtr request()
        {
            ix::HttpClient client;
            ix::SocketTLSOptions options;
            options.caFile = tlsServerCredential("ca.pem");
            client.setTLSOptions(options);
            auto args = client.createRequest();
            args->connectTimeout = 1;
            args->transferTimeout = 1;
            args->compress = false;
            return client.get("https://127.0.0.1:" + std::to_string(server.getPort()) + "/", args);
        }

    private:
        std::mutex _mutex;
        std::condition_variable _changed;
        unsigned _accepted = 0;

    public:
        std::atomic<unsigned> timeouts {0};
        ix::HttpServer server;
    };
} // namespace

TEST_CASE("An idle TLS peer does not block other handshakes or shutdown", "[tls][server]")
{
    TLSAcceptTestServer fixture;
    fixture.start();
    ix::Socket peer;
    fixture.connectIdlePeer(peer);
    REQUIRE(fixture.waitForAccepted(1));

    auto response = fixture.request();
    INFO(response->errorMsg);
    REQUIRE(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(response->statusCode == 200);
    CHECK(response->body == "ok");

    const auto start = Clock::now();
    fixture.server.stop();
    CHECK(Clock::now() - start < std::chrono::seconds(1));
}

TEST_CASE("Pending TLS handshakes count toward the connection limit", "[tls][server]")
{
    TLSAcceptTestServer fixture(1);
    fixture.start();
    ix::Socket peer;
    fixture.connectIdlePeer(peer);
    REQUIRE(fixture.waitForAccepted(1));
    CHECK(fixture.request()->errorCode != ix::HttpErrorCode::Ok);

    // A failed handshake must release its slot so that a valid client can connect.
    peer.close();
    const auto deadline = Clock::now() + std::chrono::seconds(3);
    ix::HttpResponsePtr response;
    do
    {
        response = fixture.request();
        if (response->errorCode == ix::HttpErrorCode::Ok) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (Clock::now() < deadline);
    INFO(response->errorMsg);
    CHECK(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(response->statusCode == 200);
}

TEST_CASE("Idle TLS handshakes time out", "[tls][server]")
{
    TLSAcceptTestServer fixture(1);
    fixture.start();
    ix::Socket peer;
    fixture.connectIdlePeer(peer);
    REQUIRE(fixture.waitForAccepted(1));

    const auto start = Clock::now();
    const auto deadline = start + std::chrono::seconds(12);
    char byte;
    CHECK_FALSE(peer.readByte(&byte, [deadline] { return Clock::now() >= deadline; }));
    CHECK(Clock::now() - start < std::chrono::seconds(11));
    CHECK(fixture.timeouts == 1);
}
