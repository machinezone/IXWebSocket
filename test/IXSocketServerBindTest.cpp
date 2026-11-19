/*
 * IXSocketServerBindTest.cpp
 * Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <catch_amalgamated.hpp>
#include <chrono>
#include <future>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXHttpServer.h>
#include <ixwebsocket/IXWebSocketServer.h>

namespace
{
    ix::SocketTLSOptions tlsOptions(bool server)
    {
        ix::SocketTLSOptions options;
#ifdef IX_SERVER_BIND_TEST_CERTS
        options.caFile = std::string(IX_SERVER_BIND_TEST_CERTS) + "/ca.pem";
        if (server)
        {
            options.tls = true;
            options.certFile = std::string(IX_SERVER_BIND_TEST_CERTS) + "/server.pem";
            options.keyFile = std::string(IX_SERVER_BIND_TEST_CERTS) + "/server.key";
            options.caFile = "NONE";
        }
#else
        (void) server;
#endif
        return options;
    }

    void configure(ix::HttpServer& server)
    {
        server.setOnConnectionCallback(
            [](ix::HttpRequestPtr, std::shared_ptr<ix::ConnectionState>)
            {
                return std::make_shared<ix::HttpResponse>(
                    200, "OK", ix::HttpErrorCode::Ok, ix::WebSocketHttpHeaders(), "bound");
            });
    }

    void configure(ix::WebSocketServer& server)
    {
        server.setOnClientMessageCallback([](std::shared_ptr<ix::ConnectionState>,
                                             ix::WebSocket&,
                                             const ix::WebSocketMessagePtr&) { });
    }

    void checkConnection(ix::HttpServer& server, bool tls)
    {
        ix::HttpClient client;
        if (tls)
        {
            client.setTLSOptions(tlsOptions(false));
        }
        auto args = client.createRequest();
        args->connectTimeout = 1;
        args->transferTimeout = 1;
        const auto response = client.get(std::string(tls ? "https" : "http") +
                                             "://127.0.0.1:" + std::to_string(server.getPort()),
                                         args);
        INFO(response->errorMsg);
        REQUIRE(response->errorCode == ix::HttpErrorCode::Ok);
        CHECK(response->statusCode == 200);
        CHECK(response->body == "bound");
    }

    void checkConnection(ix::WebSocketServer& server, bool tls)
    {
        std::promise<ix::WebSocketMessageType> connected;
        auto result = connected.get_future();
        ix::WebSocket client;
        client.disableAutomaticReconnection();
        if (tls)
        {
            client.setTLSOptions(tlsOptions(false));
        }
        client.setUrl(std::string(tls ? "wss" : "ws") +
                      "://127.0.0.1:" + std::to_string(server.getPort()));
        client.setOnMessageCallback(
            [&](const ix::WebSocketMessagePtr& message)
            {
                if (message->type == ix::WebSocketMessageType::Open ||
                    message->type == ix::WebSocketMessageType::Error)
                {
                    connected.set_value(message->type);
                }
            });
        client.start();
        REQUIRE(result.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
        CHECK(result.get() == ix::WebSocketMessageType::Open);
        client.stop();
    }
} // namespace

TEMPLATE_TEST_CASE("Servers can configure and retry their bind address",
                   "[server][bind]",
                   ix::HttpServer,
                   ix::WebSocketServer)
{
#ifdef IX_SERVER_BIND_TEST_CERTS
    const bool tls = GENERATE(false, true);
#else
    const bool tls = false;
#endif
    TestType server;
    configure(server);
    if (tls)
    {
        server.setTLSOptions(tlsOptions(true));
    }
    CHECK(server.getPort() == ix::SocketServer::kDefaultPort);
    REQUIRE(server.setPort(0));
    REQUIRE(server.setHost("invalid-address"));
    CHECK_FALSE(server.listen().first);
    CHECK_FALSE(server.listen().first); // Retrying must not reinitialize an existing pipe.
    REQUIRE(server.setHost("127.0.0.1"));
#ifndef _WIN32
    // Hold a real listener open, so the failure doesn't depend on port selection races.
    ix::HttpServer occupied(0);
    REQUIRE(occupied.listen().first);
    REQUIRE(server.setPort(occupied.getPort()));
    CHECK_FALSE(server.listen().first);
    CHECK_FALSE(server.listen().first);
    REQUIRE(server.setPort(0));
#endif
    auto listening = server.listen();
    INFO(listening.second);
    REQUIRE(listening.first);
    REQUIRE(server.getPort() > 0);
    const int port = server.getPort();
    CHECK_FALSE(server.setPort(12345));
    CHECK_FALSE(server.setHost("0.0.0.0"));
    CHECK_FALSE(server.listen().first); // Preserve the live listener.
    CHECK(server.getPort() == port);
    CHECK(server.getHost() == "127.0.0.1");
    server.start();
    CHECK_FALSE(server.setPort(0));
    CHECK_FALSE(server.setHost("0.0.0.0"));
    checkConnection(server, tls);
    server.stop();

    // A fully stopped instance can listen again with a new configuration.
    REQUIRE(server.setHost("0.0.0.0"));
    REQUIRE(server.setPort(0));
    REQUIRE(server.listen().first);
    server.start();
    checkConnection(server, tls);
    server.stop();
    server.stop();
}

TEST_CASE("Server port setters reject out-of-range ports", "[server][bind]")
{
    ix::HttpServer server(12345);
    CHECK_FALSE(server.setPort(-1));
    CHECK_FALSE(server.setPort(65536));
    CHECK(server.getPort() == 12345);
    CHECK(server.setPort(65535));
    CHECK(server.getPort() == 65535);
}

TEST_CASE("Servers can reconfigure after stopping an unstarted listener", "[server][bind]")
{
#ifdef IX_SERVER_BIND_TEST_CERTS
    const bool tls = GENERATE(false, true);
#else
    const bool tls = false;
#endif
    ix::HttpServer server(0);
    configure(server);
    if (tls)
    {
        server.setTLSOptions(tlsOptions(true));
    }
    REQUIRE(server.listen().first);
    server.stop();
    REQUIRE(server.setHost("0.0.0.0"));
    REQUIRE(server.setPort(0));
    REQUIRE(server.listen().first);
    server.start();
    checkConnection(server, tls);
    server.stop();
}

TEST_CASE("IPv6 listeners report the OS-assigned port", "[server][bind][ipv6]")
{
    ix::HttpServer server(0, "::1", 5, 128, AF_INET6);
    REQUIRE(server.listen().first);
    CHECK(server.getPort() > 0);
}
