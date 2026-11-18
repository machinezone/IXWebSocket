/*
 * IXHttpWebSocketUpgradeTest.cpp
 * Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <catch_amalgamated.hpp>
#include <future>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXSocketServer.h>
#include <ixwebsocket/IXWebSocket.h>
#include <mutex>

namespace
{
    class UpgradeServer final : public ix::SocketServer
    {
    public:
        struct Request
        {
            unsigned connection;
            std::string uri;
            ix::WebSocketHttpHeaders headers;
        };
        explicit UpgradeServer(bool tls = false,
                               std::string mode = "normal",
                               std::function<void()> beforeAuthReply = {},
                               std::function<void()> beforeUpgradeReply = {})
            : SocketServer(ix::getFreePort(), "127.0.0.1")
            , _tls(tls)
            , _mode(std::move(mode))
            , _beforeAuthReply(std::move(beforeAuthReply))
            , _beforeUpgradeReply(std::move(beforeUpgradeReply))
        {
#ifdef IX_HTTP_UPGRADE_TEST_CERTS
            if (tls)
            {
                ix::SocketTLSOptions options;
                options.tls = true;
                options.certFile = std::string(IX_HTTP_UPGRADE_TEST_CERTS) + "/server.pem";
                options.keyFile = std::string(IX_HTTP_UPGRADE_TEST_CERTS) + "/server.key";
                options.caFile = "NONE";
                setTLSOptions(options);
            }
#endif
            REQUIRE(listen().first);
            start();
        }
        ~UpgradeServer() override
        {
            stop();
        }
        std::string httpUrl()
        {
            return std::string(_tls ? "https" : "http") +
                   "://127.0.0.1:" + std::to_string(getPort());
        }
        std::string wsUrl()
        {
            return std::string(_tls ? "wss" : "ws") + "://127.0.0.1:" + std::to_string(getPort());
        }
        unsigned connections() const
        {
            return _connections;
        }
        std::vector<Request> requests()
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _requests;
        }

    private:
        void handleConnection(std::unique_ptr<ix::Socket> socket,
                              std::shared_ptr<ix::ConnectionState> state) override
        {
            const unsigned connection = ++_connections;
            bool authenticated = false;
            while (true)
            {
                const auto parsed = ix::Http::parseRequest(socket, 1);
                if (!std::get<0>(parsed))
                {
                    break;
                }
                const auto request = std::get<2>(parsed);
                {
                    std::lock_guard<std::mutex> lock(_mutex);
                    _requests.push_back({connection, request->uri, request->headers});
                }
                if (request->uri == "/auth")
                {
                    if (_beforeAuthReply)
                    {
                        _beforeAuthReply();
                    }
                    authenticated = true;
                    const auto deadline =
                        std::chrono::steady_clock::now() + std::chrono::seconds(1);
                    const std::string header = _mode == "http-close" ? "Connection: close\r\n" : "";
                    if (!socket->writeBytes(
                            "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n" + header + "\r\ntoken" +
                                (_mode == "pending-data" ? "X" : ""),
                            [deadline] { return std::chrono::steady_clock::now() >= deadline; }) ||
                        _mode == "http-close" || _mode == "stale")
                    {
                        break;
                    }
                    continue;
                }
                if (request->uri == "/classic" ||
                    (authenticated && request->headers["Authorization"] == "Bearer token"))
                {
                    if (_mode == "wait-for-close")
                    {
                        _beforeUpgradeReply();
                        const auto deadline =
                            std::chrono::steady_clock::now() + std::chrono::seconds(1);
                        socket->readLine([deadline]
                                         { return std::chrono::steady_clock::now() >= deadline; });
                        break;
                    }
                    if (_mode == "drop")
                    {
                        break;
                    }
                    if (_mode == "redirect" || _mode == "reject" || _mode == "bad-accept")
                    {
                        const std::string reply =
                            _mode == "bad-accept"
                                ? "HTTP/1.1 101 Switching Protocols\r\nUpgrade: "
                                  "websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "
                                  "invalid\r\n\r\n"
                                : std::string("HTTP/1.1 ") +
                                      (_mode == "redirect" ? "302 Found" : "401 Unauthorized") +
                                      "\r\nLocation: " + wsUrl() +
                                      "/classic\r\nContent-Length: 0\r\nX-Upgrade-Error: "
                                      "retained\r\n\r\n";
                        socket->writeBytes(reply, nullptr);
                        break;
                    }
                    using Transport = ix::WebSocketTransport;
                    Transport transport;
                    transport.configure(ix::WebSocketPerMessageDeflateOptions(), {}, true, -1);
                    if (transport.connectToSocket(std::move(socket), 1, true, request).success)
                    {
                        // Send immediately, possibly in the same TLS record/read as the 101.
                        transport.sendText(std::string("ready"), nullptr);
                        while (transport.getReadyState() != Transport::ReadyState::CLOSED)
                        {
                            transport.dispatch(transport.poll(),
                                               [&](const std::string& data,
                                                   size_t,
                                                   bool,
                                                   Transport::MessageKind kind)
                                               {
                                                   if (kind == Transport::MessageKind::MSG_TEXT)
                                                   {
                                                       transport.sendText(data, nullptr);
                                                   }
                                                   if (kind == Transport::MessageKind::MSG_BINARY)
                                                   {
                                                       transport.sendBinary(data, nullptr);
                                                   }
                                               });
                        }
                    }
                    break;
                }
                socket->writeBytes(
                    "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
                    nullptr);
                break;
            }
            state->setTerminated();
        }
        size_t getConnectedClientsCount() override
        {
            return 0;
        }
        bool _tls;
        std::string _mode;
        std::function<void()> _beforeAuthReply;
        std::function<void()> _beforeUpgradeReply;
        std::atomic<unsigned> _connections {0};
        std::mutex _mutex;
        std::vector<Request> _requests;
    };

    ix::HttpRequestArgsPtr options(ix::HttpClient& client)
    {
        auto args = client.createRequest();
        args->connectTimeout = 1;
        args->transferTimeout = 1;
        args->compress = false;
        return args;
    }
    void configure(ix::HttpClient& client)
    {
#ifdef IX_HTTP_UPGRADE_TEST_CERTS
        ix::SocketTLSOptions tls;
        tls.caFile = std::string(IX_HTTP_UPGRADE_TEST_CERTS) + "/ca.pem";
        client.setTLSOptions(tls);
#else
        (void) client;
#endif
    }

    struct Client
    {
        std::promise<void> opened;
        std::promise<void> ready;
        std::promise<std::pair<std::string, bool>> echoed;
        std::promise<uint16_t> closed;
        std::future<void> open = opened.get_future();
        std::future<void> welcome = ready.get_future();
        std::future<std::pair<std::string, bool>> echo = echoed.get_future();
        std::future<uint16_t> close = closed.get_future();
        std::atomic<unsigned> opens {0}, welcomes {0}, echoes {0}, closes {0};
        std::thread::id openThread;
        ix::WebSocket ws;
        Client()
        {
            ws.setExtraHeaders({{"Authorization", "Bearer token"}});
            ws.setOnMessageCallback(
                [this](const ix::WebSocketMessagePtr& message)
                {
                    if (message->type == ix::WebSocketMessageType::Open && ++opens == 1)
                    {
                        openThread = std::this_thread::get_id();
                        opened.set_value();
                    }
                    if (message->type == ix::WebSocketMessageType::Message)
                    {
                        if (message->str == "ready")
                        {
                            if (++welcomes == 1)
                            {
                                ready.set_value();
                            }
                        }
                        else if (++echoes == 1)
                        {
                            echoed.set_value({message->str, message->binary});
                        }
                    }
                    if (message->type == ix::WebSocketMessageType::Close && ++closes == 1)
                    {
                        closed.set_value(message->closeInfo.code);
                    }
                });
        }
        ~Client()
        {
            ws.stop();
        }
        void checkEcho()
        {
            ws.start();
            REQUIRE(welcome.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
            const std::string payload("hello\0world", 11);
            REQUIRE(ws.sendBinary(payload).success);
            REQUIRE(echo.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
            const auto result = echo.get();
            CHECK(result.first == payload);
            CHECK(result.second);
        }
    };
} // namespace

TEST_CASE("HTTP authentication and WebSocket messages share one TCP and TLS connection",
          "[http][upgrade]")
{
#ifdef IX_HTTP_UPGRADE_TEST_CERTS
    const bool tls = GENERATE(false, true);
#else
    const bool tls = false;
#endif
    const bool destroyHttp = GENERATE(false, true);
    UpgradeServer server(tls);
    std::unique_ptr<ix::HttpClient> http(new ix::HttpClient());
    configure(*http);
    REQUIRE(http->get(server.httpUrl() + "/auth", options(*http))->errorCode ==
            ix::HttpErrorCode::Ok);
    REQUIRE(http->get(server.httpUrl() + "/auth", options(*http))->errorCode ==
            ix::HttpErrorCode::Ok);
    Client client;
    client.ws.setUrl(server.wsUrl() + "/ws");
    client.ws.setDisabledDefaultHeaders({"User-Agent", "Origin"});
    client.ws.addSubProtocol("test-protocol");
    const auto status = client.ws.connect(*http, 1);
    INFO(status.errorStr);
    REQUIRE(status.success);
    CHECK(status.http_status == 101);
    CHECK_FALSE(client.ws.isAutomaticReconnectionEnabled());
    CHECK(client.openThread == std::this_thread::get_id());
    const auto requests = server.requests();
    REQUIRE(requests.size() == 3);
    CHECK(requests[0].connection == requests[1].connection);
    CHECK(requests[1].connection == requests[2].connection);
    CHECK(requests[2].headers.at("Sec-WebSocket-Protocol") == "test-protocol");
    CHECK(requests[2].headers.count("User-Agent") == 0);
    CHECK(requests[2].headers.count("Origin") == 0);
    if (destroyHttp)
    {
        http.reset();
    }
    client.checkEcho();
    CHECK(server.connections() == 1);
    if (!destroyHttp)
    {
        // A later HTTP request uses a new connection without disturbing the WebSocket.
        REQUIRE(http->get(server.httpUrl() + "/auth", options(*http))->errorCode ==
                ix::HttpErrorCode::Ok);
        CHECK(server.connections() == 2);
    }
    client.ws.stop();
    CHECK(client.closes == 1);
}

TEST_CASE("Classic WebSocket start still opens only one connection", "[http][upgrade]")
{
#ifdef IX_HTTP_UPGRADE_TEST_CERTS
    const bool tls = GENERATE(false, true);
#else
    const bool tls = false;
#endif
    UpgradeServer server(tls);
    Client client;
    client.ws.setUrl(server.wsUrl() + "/classic");
    client.ws.disableAutomaticReconnection();
#ifdef IX_HTTP_UPGRADE_TEST_CERTS
    ix::SocketTLSOptions settings;
    settings.caFile = std::string(IX_HTTP_UPGRADE_TEST_CERTS) + "/ca.pem";
    client.ws.setTLSOptions(settings);
#endif
    client.checkEcho();
    CHECK(server.connections() == 1);
    CHECK(server.requests().size() == 1);
    CHECK(client.openThread != std::this_thread::get_id());
}

TEST_CASE("Upgrade destination mismatches leave the HTTP connection available", "[http][upgrade]")
{
    const std::string mismatch = GENERATE("scheme", "host", "port", "invalid", "http");
    UpgradeServer server;
    ix::HttpClient http;
    REQUIRE(http.get(server.httpUrl() + "/auth", options(http))->errorCode ==
            ix::HttpErrorCode::Ok);
    Client client;
    std::string url = server.wsUrl() + "/ws";
    if (mismatch == "scheme")
    {
        url.replace(0, 2, "wss");
    }
    if (mismatch == "host")
    {
        url = "ws://127.0.0.2:" + std::to_string(server.getPort()) + "/ws";
    }
    if (mismatch == "port")
    {
        url = "ws://127.0.0.1:" + std::to_string(ix::getFreePort()) + "/ws";
    }
    if (mismatch == "invalid")
    {
        url = "invalid";
    }
    if (mismatch == "http")
    {
        url = server.httpUrl() + "/ws";
    }
    client.ws.setUrl(url);
    CHECK_FALSE(client.ws.connect(http, 1).success);
    CHECK(client.opens == 0);
    client.ws.setUrl(server.wsUrl() + "/ws");
    REQUIRE(client.ws.connect(http, 1).success);
    client.checkEcho();
    CHECK(server.connections() == 1);
}

TEST_CASE("Failed HTTP upgrades close the handed-off connection without reconnecting",
          "[http][upgrade]")
{
    const std::string mode =
        GENERATE("redirect", "reject", "bad-accept", "drop", "pending-data", "stale", "http-close");
#ifdef IX_HTTP_UPGRADE_TEST_CERTS
    const bool tls = GENERATE(false, true);
#else
    const bool tls = false;
#endif
    UpgradeServer server(tls, mode);
    ix::HttpClient http;
    configure(http);
    REQUIRE(http.get(server.httpUrl() + "/auth", options(http))->errorCode ==
            ix::HttpErrorCode::Ok);
    Client client;
    client.ws.setUrl(server.wsUrl() + "/ws");
    const auto status = client.ws.connect(http, 1);
    CHECK_FALSE(status.success);
    CHECK(client.ws.getReadyState() == ix::ReadyState::Closed);
    CHECK(client.opens == 0);
    CHECK(server.connections() == 1);
    if (mode == "redirect" || mode == "reject")
    {
        CHECK(status.http_status == (mode == "redirect" ? 302 : 401));
        CHECK(status.headers.at("X-Upgrade-Error") == "retained");
    }
    REQUIRE(http.get(server.httpUrl() + "/auth", options(http))->errorCode ==
            ix::HttpErrorCode::Ok);
    CHECK(server.connections() == 2);
}

TEST_CASE("An upgrade requires a callback, timeout and idle HTTP connection", "[http][upgrade]")
{
    UpgradeServer server;
    ix::HttpClient http;
    Client client;
    client.ws.setUrl(server.wsUrl() + "/ws");
    CHECK_FALSE(client.ws.connect(http, 1).success);
    CHECK(server.connections() == 0);
    REQUIRE(http.get(server.httpUrl() + "/auth", options(http))->errorCode ==
            ix::HttpErrorCode::Ok);
    CHECK_FALSE(client.ws.connect(http, 0).success);
    client.ws.setOnMessageCallback(nullptr);
    CHECK_FALSE(client.ws.connect(http, 1).success);
    client.ws.setOnMessageCallback([](const ix::WebSocketMessagePtr&) { });
    REQUIRE(client.ws.connect(http, 1).success);
    CHECK_FALSE(client.ws.connect(http, 1).success); // Already open; leave it intact.
    client.ws.start();
    client.ws.stop();
    CHECK(server.connections() == 1);
}

TEST_CASE("HTTP-to-WebSocket handoff refuses a busy asynchronous HTTP client", "[http][upgrade]")
{
    std::promise<void> arrived;
    auto arrivedFuture = arrived.get_future();
    std::promise<void> release;
    auto releaseFuture = release.get_future();
    UpgradeServer server(false,
                         "normal",
                         [&]
                         {
                             arrived.set_value();
                             releaseFuture.wait_for(std::chrono::seconds(1));
                         });
    std::promise<ix::HttpResponsePtr> response;
    auto completed = response.get_future();
    ix::HttpClient http(true);
    auto args = options(http);
    args->url = server.httpUrl() + "/auth";
    args->verb = "GET";
    REQUIRE(http.performRequest(
        args, [&](const ix::HttpResponsePtr& result) { response.set_value(result); }));
    REQUIRE(arrivedFuture.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    Client client;
    client.ws.setUrl(server.wsUrl() + "/ws");
    const auto status = client.ws.connect(http, 1);
    CHECK_FALSE(status.success);
    CHECK(status.errorStr.find("busy") != std::string::npos);
    release.set_value();
    REQUIRE(completed.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    REQUIRE(completed.get()->errorCode == ix::HttpErrorCode::Ok);
    REQUIRE(client.ws.connect(http, 1).success);
    client.checkEcho();
    CHECK(server.connections() == 1);
}

TEST_CASE("HTTP-upgraded WebSockets retain their configured message limit", "[http][upgrade]")
{
    UpgradeServer server;
    ix::HttpClient http;
    REQUIRE(http.get(server.httpUrl() + "/auth", options(http))->errorCode ==
            ix::HttpErrorCode::Ok);
    Client client;
    client.ws.setUrl(server.wsUrl() + "/ws");
    client.ws.setMaxMessageSize(4); // The server immediately sends the five-byte message 'ready'.
    REQUIRE(client.ws.connect(http, 1).success);
    client.ws.start();
    REQUIRE(client.close.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    CHECK(client.close.get() == 1009);
    client.ws.stop();
    CHECK(server.connections() == 1);
    CHECK(client.opens == 1);
    CHECK_FALSE(client.ws.isAutomaticReconnectionEnabled());
}

TEST_CASE("Closing cancels an HTTP-to-WebSocket handshake without reconnecting",
          "[http][upgrade][close]")
{
#ifdef IX_HTTP_UPGRADE_TEST_CERTS
    const bool tls = GENERATE(false, true);
#else
    const bool tls = false;
#endif
    std::promise<void> arrived;
    auto requestSeen = arrived.get_future();
    UpgradeServer server(tls, "wait-for-close", {}, [&] { arrived.set_value(); });
    ix::HttpClient http;
    configure(http);
    REQUIRE(http.get(server.httpUrl() + "/auth", options(http))->errorCode ==
            ix::HttpErrorCode::Ok);
    Client client;
    client.ws.setUrl(server.wsUrl() + "/ws");
    auto connected = std::async(std::launch::async, [&] { return client.ws.connect(http, 1); });
    REQUIRE(requestSeen.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    client.ws.stop();
    REQUIRE(connected.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    CHECK_FALSE(connected.get().success);
    CHECK(client.ws.getReadyState() == ix::ReadyState::Closed);
    CHECK(client.opens == 0);
    CHECK(server.connections() == 1);
}
