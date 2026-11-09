#include "IXTest.h"
#include <catch_amalgamated.hpp>
#include <fstream>
#include <future>
#include <iterator>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXHttpServer.h>
#include <ixwebsocket/IXSelectInterrupt.h>
#include <ixwebsocket/IXSocketNetwork.h>

using namespace ix;
namespace
{
    std::string credential(const std::string& name)
    {
        return std::string(IX_NETWORK_TEST_CERTS) + "/" + name;
    }
    SocketTLSOptions serverOptions(const std::string& name = "server")
    {
        SocketTLSOptions options;
        options.tls = true;
        options.certFile = credential(name + ".pem");
        options.keyFile = credential(name + ".key");
        options.caFile = "NONE";
        return options;
    }
    SocketTLSOptions clientOptions()
    {
        SocketTLSOptions options;
        options.caFile = credential("ca.pem");
        return options;
    }
    void checkRequest(SocketTLSOptions serverTLS,
                      SocketTLSOptions clientTLS,
                      bool expected,
                      const std::string& host = "127.0.0.1",
                      int family = AF_INET,
                      const std::string& peerHost = "")
    {
        HttpServer server(0, host, 5, 128, family);
        server.setTLSOptions(serverTLS);
        // Exercise several read/send buffers, including the final send before close.
        const std::string body(256 * 1024, 'x');
        server.setOnConnectionCallback(
            [&](HttpRequestPtr request, std::shared_ptr<ConnectionState>)
            {
                return std::make_shared<HttpResponse>(
                    200, "OK", HttpErrorCode::Ok, WebSocketHttpHeaders(), request->body);
            });
        server.setLogCallback([](LogLevel, const std::string&) { });
        auto listening = server.listen();
        INFO(listening.second);
        REQUIRE(listening.first);
        server.start();
        HttpClient client;
        client.setTLSOptions(clientTLS);
        std::string target = peerHost.empty() ? host : peerHost;
        std::string url = "https://" + (family == AF_INET6 ? "[" + target + "]" : target) + ":" +
                          std::to_string(server.getPort()) + "/";
        auto args = client.createRequest(url);
        args->connectTimeout = 3;
        args->transferTimeout = 5;
        args->compress = false;
        auto response = client.post(url, body, args);
        INFO(response->errorMsg);
        CHECK((response->errorCode == HttpErrorCode::Ok) == expected);
        if (expected)
        {
            CHECK(response->statusCode == 200);
            CHECK(response->body.size() == body.size());
            CHECK(bool(response->body == body));
        }
        server.stop();
    }
} // namespace

TEST_CASE("Network.framework TLS", "[network_tls]")
{
    SECTION("custom CA and IPv4 SAN")
    {
        checkRequest(serverOptions(), clientOptions(), true);
    }
    SECTION("intermediate certificate chain")
    {
        checkRequest(serverOptions("chain"), clientOptions(), true);
    }
    SECTION("DNS hostname")
    {
        checkRequest(serverOptions(), clientOptions(), true, "127.0.0.1", AF_INET, "localhost");
    }
    SECTION("custom CA and IPv6 SAN")
    {
        checkRequest(serverOptions(), clientOptions(), true, "::1", AF_INET6);
    }
    SECTION("wrong hostname")
    {
        checkRequest(serverOptions("wrong"), clientOptions(), false);
    }
    SECTION("hostname checking can be disabled")
    {
        auto client = clientOptions();
        client.disable_hostname_validation = true;
        checkRequest(serverOptions("wrong"), client, true);
    }
    SECTION("expired certificate")
    {
        auto client = clientOptions();
        client.caFile = credential("expired-ca.pem");
        checkRequest(serverOptions("expired"), client, false, "127.0.0.1", AF_INET, "localhost");
    }
    SECTION("SYSTEM rejects an untrusted private CA")
    {
        SocketTLSOptions client;
        checkRequest(serverOptions(), client, false);
    }
    SECTION("NONE disables verification")
    {
        auto client = clientOptions();
        client.caFile = "NONE";
        checkRequest(serverOptions("expired"), client, true);
    }
    SECTION("in-memory CA")
    {
        auto client = clientOptions();
        std::ifstream file(credential("ca.pem"));
        client.caFile =
            "# CA bundle with comments\n" + std::string(std::istreambuf_iterator<char>(file), {});
        checkRequest(serverOptions(), client, true);
    }
    SECTION("mutual TLS")
    {
        auto server = serverOptions();
        server.caFile = credential("ca.pem");
        auto client = clientOptions();
        client.certFile = credential("client.pem");
        client.keyFile = credential("client.key");
        checkRequest(server, client, true);
    }
    SECTION("missing client certificate")
    {
        auto server = serverOptions();
        server.caFile = credential("ca.pem");
        checkRequest(server, clientOptions(), false);
    }
    SECTION("untrusted client certificate")
    {
        auto server = serverOptions();
        server.caFile = credential("ca.pem");
        auto client = clientOptions();
        client.certFile = credential("untrusted.pem");
        client.keyFile = credential("untrusted.key");
        checkRequest(server, client, false);
    }
    SECTION("a server certificate cannot authenticate a client")
    {
        auto server = serverOptions();
        server.caFile = credential("ca.pem");
        auto client = clientOptions();
        client.certFile = credential("server.pem");
        client.keyFile = credential("server.key");
        checkRequest(server, client, false);
    }
    SECTION("invalid CA fails closed")
    {
        auto options = clientOptions();
        options.caFile = credential("client.key");
        SocketNetwork socket(options);
        std::string error;
        REQUIRE(socket.init(error));
        CHECK_FALSE(socket.connect("127.0.0.1", 443, error, [] { return false; }));
        CHECK_FALSE(error.empty());
    }
    SECTION("unsupported cipher strings fail explicitly")
    {
        auto options = serverOptions();
        options.ciphers = "ECDHE-RSA-AES128-GCM-SHA256";
        HttpServer server(0);
        server.setTLSOptions(options);
        auto result = server.listen();
        CHECK_FALSE(result.first);
        CHECK(result.second.find("cipher") != std::string::npos);
    }
    SECTION("bad identity fails at listen")
    {
        auto options = serverOptions();
        options.keyFile = credential("client.key");
        HttpServer server(0);
        server.setTLSOptions(options);
        auto result = server.listen();
        CHECK_FALSE(result.first);
        CHECK_FALSE(result.second.empty());
    }
    SECTION("poll can be interrupted")
    {
        SocketNetwork socket(clientOptions());
        std::string error;
        REQUIRE(socket.init(error));
        auto polling = std::async(std::launch::async, [&] { return socket.isReadyToRead(3000); });
        REQUIRE(socket.wakeUpFromPoll(SelectInterrupt::kCloseRequest));
        CHECK(polling.get() == PollResultType::CloseRequest);
    }
    SECTION("connection cancellation")
    {
        SocketNetwork socket(clientOptions());
        std::string error;
        REQUIRE(socket.init(error));
        CHECK_FALSE(socket.connect("192.0.2.1", 443, error, [] { return true; }));
        CHECK_FALSE(error.empty());
    }
}

TEST_CASE("Network.framework WebSocket echo", "[network_tls]")
{
    WebSocketServer server(0, "127.0.0.1");
    server.setTLSOptions(serverOptions());
    server.setOnClientMessageCallback(
        [](std::shared_ptr<ConnectionState>, WebSocket& socket, const WebSocketMessagePtr& message)
        {
            if (message->type == WebSocketMessageType::Message) socket.sendBinary(message->str);
        });
    auto listening = server.listen();
    INFO(listening.second);
    REQUIRE(listening.first);
    server.start();
    WebSocket client;
    client.setTLSOptions(clientOptions());
    client.setUrl("wss://127.0.0.1:" + std::to_string(server.getPort()) + "/");
    std::promise<std::string> received;
    std::atomic<bool> completed(false);
    const std::string body(256 * 1024, 'z');
    client.disableAutomaticReconnection();
    client.setOnMessageCallback(
        [&](const WebSocketMessagePtr& message)
        {
            if (message->type == WebSocketMessageType::Open) client.sendBinary(body);
            if (message->type == WebSocketMessageType::Message && !completed.exchange(true))
                received.set_value(message->str);
            if (message->type == WebSocketMessageType::Error && !completed.exchange(true))
                received.set_value(message->errorInfo.reason);
        });
    auto result = received.get_future();
    client.start();
    auto status = result.wait_for(std::chrono::seconds(5));
    CHECK(status == std::future_status::ready);
    if (status == std::future_status::ready) CHECK(bool(result.get() == body));
    client.stop();
    server.stop();
}

TEST_CASE("Network.framework server stop cancels a stalled handshake", "[network_tls]")
{
    HttpServer server(0, "127.0.0.1");
    server.setTLSOptions(serverOptions());
    server.setLogCallback([](LogLevel, const std::string&) { });
    REQUIRE(server.listen().first);
    server.start();
    Socket peer;
    std::string error;
    REQUIRE(peer.init(error));
    REQUIRE(peer.connect("127.0.0.1", server.getPort(), error, [] { return false; }));
    msleep(50);
    auto start = std::chrono::steady_clock::now();
    server.stop();
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(1));
}
