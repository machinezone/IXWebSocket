/*
 *  IXSocks5Test.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <atomic>
#include <catch_amalgamated.hpp>
#include <chrono>
#include <future>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXHttpServer.h>
#include <ixwebsocket/IXSocketFactory.h>
#include <mutex>
#include <thread>

namespace
{
    using Clock = std::chrono::steady_clock;
    enum class Reply
    {
        IPv4,
        Domain,
        IPv6,
        Truncated,
        Version,
        Method,
        UnofferedAuth,
        ConnectVersion,
        Reserved,
        Auth,
        AuthVersion,
        Rejected,
        AddressType,
        EmptyDomain,
        Stall
    };

    class SocksProxy final : public ix::SocketServer
    {
    public:
        struct Request
        {
            unsigned addressType = 0;
            std::string host;
            int port = 0;
            std::string username;
            std::string password;
        };
        explicit SocksProxy(Reply reply = Reply::IPv4, bool auth = false, bool tunnel = false)
            : SocketServer(ix::getFreePort(), "127.0.0.1")
            , _reply(reply)
            , _auth(auth)
            , _tunnel(tunnel)
        {
        }
        ~SocksProxy() override
        {
            stop();
        }
        void stop() override
        {
            _stopping = true;
            SocketServer::stop();
        }
        void listenAndStart()
        {
            REQUIRE(listen().first);
            start();
        }
        ix::ProxyOptions options()
        {
            ix::ProxyOptions result;
            result.host = "127.0.0.1";
            result.port = getPort();
            if (_auth)
            {
                result.username = "test-user";
                result.password = "test-password";
            }
            return result;
        }
        Request request()
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _request;
        }
        std::atomic<unsigned> connections {0};
        std::atomic<unsigned> greetings {0};

    private:
        size_t getConnectedClientsCount() override
        {
            return 0;
        }
        void handleConnection(std::unique_ptr<ix::Socket> socket,
                              std::shared_ptr<ix::ConnectionState> state) override
        {
            ++connections;
            serve(*socket);
            state->setTerminated();
        }
        void serve(ix::Socket& socket)
        {
            const auto deadline = Clock::now() + std::chrono::seconds(5);
            auto cancelled = [&] { return _stopping || Clock::now() >= deadline; };
            auto read = [&](size_t size, std::string& value)
            {
                auto result = socket.readBytes(size, nullptr, nullptr, cancelled);
                value = result.second;
                return result.first;
            };
            auto write = [&](const std::string& value)
            {
                // Fragment unit-test replies; relay integration uses normal writes.
                if (_tunnel) return socket.writeBytes(value, cancelled);
                for (char byte : value)
                {
                    if (!socket.writeBytes(std::string(1, byte), cancelled)) return false;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                return true;
            };
            std::string header, value;
            if (!read(2, header) || !read(static_cast<uint8_t>(header[1]), value)) return;
            ++greetings;
            if (_reply == Reply::Stall)
            {
                while (!cancelled())
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return;
            }
            if (_reply == Reply::Version)
            {
                write(std::string("\x04\x00", 2));
                return;
            }
            if (_reply == Reply::Method)
            {
                write(std::string("\x05\x7f", 2));
                return;
            }
            if (_reply == Reply::UnofferedAuth)
            {
                write(std::string("\x05\x02", 2));
                return;
            }
            Request request;
            if (!write(std::string("\x05", 1) + char(_auth ? 2 : 0))) return;
            if (_auth)
            {
                if (!read(2, header) || !read(static_cast<uint8_t>(header[1]), request.username) ||
                    !read(1, header) || !read(static_cast<uint8_t>(header[0]), request.password))
                    return;
                if (!write(std::string(_reply == Reply::AuthVersion ? "\x02" : "\x01", 1) +
                           char(_reply == Reply::Auth ? 1 : 0)))
                    return;
                if (_reply == Reply::Auth || _reply == Reply::AuthVersion) return;
            }
            if (!read(4, header)) return;
            request.addressType = static_cast<uint8_t>(header[3]);
            if (request.addressType == 3)
            {
                if (!read(1, header) || !read(static_cast<uint8_t>(header[0]), request.host))
                    return;
            }
            else
            {
                if (!read(request.addressType == 1 ? 4 : 16, value)) return;
                char numeric[INET6_ADDRSTRLEN];
                if (!ix::inet_ntop(request.addressType == 1 ? AF_INET : AF_INET6,
                                   value.data(),
                                   numeric,
                                   sizeof(numeric)))
                    return;
                request.host = numeric;
            }
            if (!read(2, value)) return;
            request.port = (static_cast<uint8_t>(value[0]) << 8) | static_cast<uint8_t>(value[1]);
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _request = request;
            }
            if (_reply == Reply::ConnectVersion)
            {
                write(std::string("\x04\x00\x00\x01", 4));
                return;
            }
            if (_reply == Reply::Reserved)
            {
                write(std::string("\x05\x00\x01\x01", 4));
                return;
            }
            if (_reply == Reply::Truncated)
            {
                write(std::string("\x05\x00\x00\x01\x7f", 5));
                return;
            }
            if (_reply == Reply::Rejected)
            {
                write(std::string("\x05\x05\x00\x01", 4));
                return;
            }
            if (_reply == Reply::AddressType)
            {
                write(std::string("\x05\x00\x00\x7f", 4));
                return;
            }
            if (_reply == Reply::EmptyDomain)
            {
                write(std::string("\x05\x00\x00\x03\x00", 5));
                return;
            }
            std::string bound;
            if (_reply == Reply::IPv4)
                bound = std::string("\x01\x7f\x00\x00\x01", 5);
            else if (_reply == Reply::Domain)
                bound = std::string("\x03\x09localhost", 11);
            else
                bound = std::string("\x04", 1) + std::string(15, '\0') + '\x01';
            std::string reply = std::string("\x05\x00\x00", 3) + bound + std::string("\x00\x50", 2);
            std::string error;
            auto upstream = _tunnel ? ix::createSocket(false, -1, error, {}) : nullptr;
            if (_tunnel &&
                (!upstream || !upstream->connect("127.0.0.1", request.port, error, cancelled)))
                return;
            if (!write(reply)) return;
            if (!_tunnel)
            {
                write("payload");
                return;
            }
            char buffer[16384];
            while (!cancelled())
            {
                for (int direction = 0; direction != 2; ++direction)
                {
                    auto& from = direction ? *upstream : socket;
                    auto& to = direction ? socket : *upstream;
                    auto count = from.recv(buffer, sizeof(buffer));
                    if (count > 0)
                    {
                        if (!to.writeBytes(std::string(buffer, static_cast<size_t>(count)),
                                           cancelled))
                            return;
                    }
                    else if (count == 0 || !ix::Socket::isWaitNeeded())
                        return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        Reply _reply;
        bool _auth;
        bool _tunnel;
        std::atomic<bool> _stopping {false};
        std::mutex _mutex;
        Request _request;
    };

    auto deadline() -> ix::CancellationRequest
    {
        auto end = Clock::now() + std::chrono::seconds(2);
        return [end] { return Clock::now() >= end; };
    }
} // namespace

TEST_CASE("SOCKS5 consumes exactly the proxy reply", "[socks5]")
{
    auto reply = GENERATE(Reply::IPv4, Reply::Domain, Reply::IPv6);
    bool auth = GENERATE(false, true);
    SocksProxy proxy(reply, auth);
    proxy.listenAndStart();
    std::string error;
    auto socket = ix::createSocket(false, -1, error, {}, proxy.options());
    REQUIRE(socket);
    REQUIRE(socket->connect("destination.invalid", 80, error, deadline()));
    auto body = socket->readBytes(7, nullptr, nullptr, deadline());
    REQUIRE(body.first);
    CHECK(body.second == "payload");
    auto request = proxy.request();
    CHECK(request.addressType == 3);
    CHECK(request.host == "destination.invalid");
    CHECK(request.port == 80);
    if (auth) CHECK(request.username == "test-user");
}

TEST_CASE("SOCKS5 rejects malformed and incomplete proxy replies", "[socks5]")
{
    auto reply = GENERATE(Reply::Truncated,
                          Reply::Version,
                          Reply::Method,
                          Reply::UnofferedAuth,
                          Reply::ConnectVersion,
                          Reply::Reserved,
                          Reply::Auth,
                          Reply::AuthVersion,
                          Reply::Rejected,
                          Reply::AddressType,
                          Reply::EmptyDomain);
    bool auth = reply == Reply::Auth || reply == Reply::AuthVersion;
    SocksProxy proxy(reply, auth);
    proxy.listenAndStart();
    std::string error;
    auto socket = ix::createSocket(false, -1, error, {}, proxy.options());
    REQUIRE(socket);
    CHECK_FALSE(socket->connect("destination.invalid", 443, error, deadline()));
    CHECK_FALSE(error.empty());
}

TEST_CASE("SOCKS5 supports local DNS and numeric destinations", "[socks5]")
{
    const std::string host =
        GENERATE(std::string("localhost"), std::string("127.0.0.1"), std::string("::1"));
    SocksProxy proxy;
    proxy.listenAndStart();
    auto options = proxy.options();
    options.dnsMode = ix::ProxyOptions::DnsMode::Local;
    std::string error;
    auto socket = ix::createSocket(false, -1, error, {}, options);
    REQUIRE(socket->connect(host, 443, error, deadline()));
    auto request = proxy.request();
    CHECK((request.addressType == 1 || request.addressType == 4));
    if (host != "localhost") CHECK(request.host == host);
}

TEST_CASE("SOCKS5 validates options before connecting", "[socks5]")
{
    SocksProxy proxy;
    proxy.listenAndStart();
    auto options = proxy.options();
    SECTION("Port")
    {
        options.port = GENERATE(-1, 0, 65536);
    }
    SECTION("Username length")
    {
        options.username.assign(256, 'u');
        options.password = "password";
    }
    SECTION("Missing password")
    {
        options.username = "user";
    }
    SECTION("DNS mode")
    {
        options.dnsMode = static_cast<ix::ProxyOptions::DnsMode>(42);
    }
    std::string error;
    auto socket = ix::createSocket(false, -1, error, {}, options);
    CHECK_FALSE(socket->connect("destination.invalid", 443, error, deadline()));
    CHECK_FALSE(error.empty());
    CHECK(proxy.connections == 0);
}

TEST_CASE("SOCKS5 connection cancellation remains responsive", "[socks5]")
{
#if defined(IX_SOCKS_TEST_CERTS) && !defined(IXWEBSOCKET_USE_NETWORK_FRAMEWORK)
    bool secure = GENERATE(false, true);
#else
    bool secure = false;
#endif
    SocksProxy proxy(Reply::Stall);
    proxy.listenAndStart();
    ix::WebSocket client;
    client.disableAutomaticReconnection();
    client.setUrl(std::string(secure ? "wss" : "ws") + "://destination.invalid/");
    client.setProxyOptions(proxy.options());
    client.setOnMessageCallback([](const ix::WebSocketMessagePtr&) { });
    client.start();
    auto end = Clock::now() + std::chrono::seconds(2);
    while (!proxy.greetings && Clock::now() < end)
        std::this_thread::yield();
    auto stop = std::async(std::launch::async, [&] { client.stop(); });
    auto ready = stop.wait_for(std::chrono::milliseconds(500));
    proxy.stop();
    stop.get();
    CHECK(proxy.greetings == 1);
    CHECK(ready == std::future_status::ready);
}

namespace
{
    bool secureAvailable()
    {
#if defined(IX_SOCKS_TEST_CERTS) && !defined(IXWEBSOCKET_USE_NETWORK_FRAMEWORK)
        return true;
#else
        return false;
#endif
    }

    void configureTLS(ix::HttpServer& server, bool secure)
    {
#ifdef IX_SOCKS_TEST_CERTS
        if (secure)
        {
            ix::SocketTLSOptions options;
            options.tls = true;
            // This certificate is valid only for wrong.invalid, not the proxy address.
            options.certFile = std::string(IX_SOCKS_TEST_CERTS) + "/wrong.pem";
            options.keyFile = std::string(IX_SOCKS_TEST_CERTS) + "/wrong.key";
            options.caFile = "NONE";
            server.setTLSOptions(options);
        }
#else
        (void) server;
        (void) secure;
#endif
    }

    ix::SocketTLSOptions clientTLS()
    {
        ix::SocketTLSOptions options;
#ifdef IX_SOCKS_TEST_CERTS
        options.caFile = std::string(IX_SOCKS_TEST_CERTS) + "/ca.pem";
#endif
        return options;
    }
} // namespace

TEST_CASE("HTTP and HTTPS retain the origin identity through SOCKS5", "[socks5][http]")
{
    bool secure = GENERATE(false, true);
    if (secure && !secureAvailable()) return;
    bool auth = GENERATE(false, true);
    ix::HttpServer origin(ix::getFreePort(), "127.0.0.1");
    configureTLS(origin, secure);
    origin.setOnConnectionCallback(
        [](ix::HttpRequestPtr request, std::shared_ptr<ix::ConnectionState>)
        {
            return std::make_shared<ix::HttpResponse>(
                200, "OK", ix::HttpErrorCode::Ok, ix::WebSocketHttpHeaders(), request->uri);
        });
    REQUIRE(origin.listen().first);
    origin.start();
    SocksProxy proxy(Reply::IPv4, auth, true);
    proxy.listenAndStart();
    ix::HttpClient client;
    client.setTLSOptions(clientTLS());
    client.setProxyOptions(proxy.options());
    auto request = client.createRequest();
    request->connectTimeout = 2;
    request->transferTimeout = 2;
    const auto url = std::string(secure ? "https" : "http") +
                     "://wrong.invalid:" + std::to_string(origin.getPort()) + "/through-proxy";
    auto response = client.get(url, request);
    proxy.stop();
    origin.stop();
    INFO(response->errorMsg);
    INFO("proxy connections: " << proxy.connections << ", greetings: " << proxy.greetings);
    INFO("target: " << proxy.request().host << ":" << proxy.request().port
                    << ", type: " << proxy.request().addressType);
    CAPTURE(secure, auth);
    REQUIRE(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(response->statusCode == 200);
    CHECK(response->body == "/through-proxy");
    CHECK(proxy.request().host == "wrong.invalid");
}

TEST_CASE("WebSocket reconnects through the configured SOCKS5 proxy", "[socks5][websocket]")
{
    bool secure = GENERATE(false, true);
    if (secure && !secureAvailable()) return;
    bool auth = GENERATE(false, true);
    ix::HttpServer origin(ix::getFreePort(), "127.0.0.1");
    configureTLS(origin, secure);
    origin.setOnClientMessageCallback(
        [](std::shared_ptr<ix::ConnectionState>,
           ix::WebSocket& socket,
           const ix::WebSocketMessagePtr& message)
        {
            if (message->type == ix::WebSocketMessageType::Message)
            {
                socket.send(message->str);
                socket.close();
            }
        });
    REQUIRE(origin.listen().first);
    origin.start();
    SocksProxy proxy(Reply::IPv4, auth, true);
    proxy.listenAndStart();
    std::promise<void> received;
    auto result = received.get_future();
    std::string error;
    unsigned count = 0;
    ix::WebSocket client;
    client.setTLSOptions(clientTLS());
    client.setProxyOptions(proxy.options());
    client.setUrl(std::string(secure ? "wss" : "ws") +
                  "://wrong.invalid:" + std::to_string(origin.getPort()) + "/");
    client.setMinWaitBetweenReconnectionRetries(1);
    client.setMaxWaitBetweenReconnectionRetries(10);
    client.setOnMessageCallback(
        [&](const ix::WebSocketMessagePtr& message)
        {
            if (message->type == ix::WebSocketMessageType::Open) client.sendText("echo");
            if (message->type == ix::WebSocketMessageType::Message && message->str == "echo" &&
                ++count == 2)
                received.set_value();
            if (message->type == ix::WebSocketMessageType::Error) error = message->errorInfo.reason;
        });
    client.start();
    auto ready = result.wait_for(std::chrono::seconds(3));
    client.stop();
    proxy.stop();
    origin.stop();
    INFO(error);
    REQUIRE(ready == std::future_status::ready);
    CHECK(proxy.connections >= 2);
}

TEST_CASE("Rejected SOCKS5 tunnels never fall back to a direct connection", "[socks5]")
{
    bool secure = GENERATE(false, true);
    if (secure && !secureAvailable()) return;
    std::atomic<unsigned> requests {0};
    ix::HttpServer origin(ix::getFreePort(), "127.0.0.1");
    configureTLS(origin, secure);
#ifdef IX_SOCKS_TEST_CERTS
    if (secure)
    {
        ix::SocketTLSOptions tls;
        tls.tls = true;
        tls.certFile = std::string(IX_SOCKS_TEST_CERTS) + "/server.pem";
        tls.keyFile = std::string(IX_SOCKS_TEST_CERTS) + "/server.key";
        tls.caFile = "NONE";
        origin.setTLSOptions(tls);
    }
#endif
    origin.setOnConnectionCallback(
        [&](ix::HttpRequestPtr, std::shared_ptr<ix::ConnectionState>)
        {
            ++requests;
            return std::make_shared<ix::HttpResponse>(200, "OK");
        });
    REQUIRE(origin.listen().first);
    origin.start();
    SocksProxy proxy(Reply::Rejected);
    proxy.listenAndStart();
    ix::HttpClient client;
    client.setTLSOptions(clientTLS());
    client.setProxyOptions(proxy.options());
    auto request = client.createRequest();
    request->connectTimeout = 2;
    request->transferTimeout = 2;
    auto response = client.get(std::string(secure ? "https" : "http") +
                                   "://localhost:" + std::to_string(origin.getPort()) + "/",
                               request);
    proxy.stop();
    origin.stop();
    CHECK(response->errorCode != ix::HttpErrorCode::Ok);
    CHECK(requests == 0);
    CHECK(proxy.connections == 1);
}

#if defined(IX_SOCKS_TEST_CERTS) && !defined(IXWEBSOCKET_USE_NETWORK_FRAMEWORK)
TEST_CASE("TLS through SOCKS5 still rejects the wrong destination certificate", "[socks5][tls]")
{
    ix::HttpServer origin(ix::getFreePort(), "127.0.0.1");
    configureTLS(origin, true);
    origin.setOnConnectionCallback([](ix::HttpRequestPtr, std::shared_ptr<ix::ConnectionState>)
                                   { return std::make_shared<ix::HttpResponse>(200, "OK"); });
    REQUIRE(origin.listen().first);
    origin.start();
    SocksProxy proxy(Reply::IPv4, false, true);
    proxy.listenAndStart();
    ix::HttpClient client;
    client.setTLSOptions(clientTLS());
    client.setProxyOptions(proxy.options());
    auto request = client.createRequest();
    request->connectTimeout = 2;
    auto response =
        client.get("https://other.invalid:" + std::to_string(origin.getPort()) + "/", request);
    proxy.stop();
    origin.stop();
    CHECK(response->errorCode != ix::HttpErrorCode::Ok);
}
#endif

#ifdef IXWEBSOCKET_USE_NETWORK_FRAMEWORK
TEST_CASE("Network.framework rejects unsupported proxy configuration", "[socks5][network_tls]")
{
    SocksProxy proxy;
    proxy.listenAndStart();
    std::string error;
    auto socket = ix::createSocket(true, -1, error, clientTLS(), proxy.options());
    REQUIRE(socket);
    CHECK_FALSE(socket->connect("localhost", 443, error, deadline()));
    CHECK(error == "SOCKS5 is not supported by the Network.framework backend");
    CHECK(proxy.connections == 0);
}
#endif

TEST_CASE("SOCKS5 bounds hostname and credential lengths", "[socks5]")
{
    const size_t size = GENERATE(size_t(255), size_t(256));
    SocksProxy proxy(Reply::IPv4, true);
    proxy.listenAndStart();
    auto options = proxy.options();
    options.username.assign(255, 'u');
    options.password.assign(255, 'p');
    std::string error;
    auto socket = ix::createSocket(false, -1, error, {}, options);
    bool connected = socket->connect(std::string(size, 'a'), 443, error, deadline());
    CHECK(connected == (size == 255));
    if (connected)
    {
        CHECK(proxy.request().host.size() == 255);
        CHECK(proxy.request().username == options.username);
        CHECK(proxy.request().password == options.password);
    }
    else
        CHECK_FALSE(error.empty());
}

TEST_CASE("SOCKS5 observes the connection deadline", "[socks5]")
{
    SocksProxy proxy(Reply::Stall);
    proxy.listenAndStart();
    std::string error;
    auto socket = ix::createSocket(false, -1, error, {}, proxy.options());
    const auto start = Clock::now();
    CHECK_FALSE(socket->connect("destination.invalid",
                                443,
                                error,
                                [start]
                                { return Clock::now() - start >= std::chrono::milliseconds(80); }));
    CHECK(Clock::now() - start < std::chrono::milliseconds(500));
    CHECK_FALSE(error.empty());
}
