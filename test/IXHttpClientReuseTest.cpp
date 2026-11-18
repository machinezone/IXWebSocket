/*
 * IXHttpClientReuseTest.cpp
 * Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <catch_amalgamated.hpp>
#include <cstring>
#include <future>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXGzipCodec.h>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXSocketServer.h>
#include <mutex>

namespace ix
{
    struct HttpClientReuseTestAccess
    {
        static void cache(HttpClient& client,
                          std::unique_ptr<Socket> socket,
                          const std::string& url)
        {
            client._socket = std::move(socket);
            client._connectionUrl = url;
        }
    };
} // namespace ix

namespace
{
    struct Reply
    {
        std::string bytes;
        bool close = false;
        explicit Reply(std::string wire, bool closeConnection = false)
            : bytes(std::move(wire))
            , close(closeConnection)
        {
        }
    };

    std::string response(const std::string& body = "ok", const std::string& headers = "")
    {
        return "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\n" +
               headers + "\r\n" + body;
    }

    class PersistentServer final : public ix::SocketServer
    {
    public:
        using Script = std::function<Reply(const ix::HttpRequestPtr&, size_t)>;
        struct Request
        {
            ix::HttpRequestPtr http;
            unsigned connection;
        };

        explicit PersistentServer(Script script = {}, bool tls = false)
            : SocketServer(ix::getFreePort(), "127.0.0.1")
            , _script(std::move(script))
            , _tls(tls)
        {
#ifdef IX_HTTP_REUSE_TEST_CERTS
            if (tls)
            {
                ix::SocketTLSOptions options;
                options.tls = true;
                options.certFile = std::string(IX_HTTP_REUSE_TEST_CERTS) + "/server.pem";
                options.keyFile = std::string(IX_HTTP_REUSE_TEST_CERTS) + "/server.key";
                options.caFile = "NONE";
                setTLSOptions(options);
            }
#endif
            REQUIRE(listen().first);
            start();
        }
        ~PersistentServer() override
        {
            stop();
        }
        std::string url()
        {
            return std::string(_tls ? "https" : "http") +
                   "://127.0.0.1:" + std::to_string(getPort()) + "/";
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
            while (true)
            {
                const auto parsed = ix::Http::parseRequest(socket, 1);
                if (!std::get<0>(parsed))
                {
                    break;
                }
                const auto request = std::get<2>(parsed);
                size_t index;
                {
                    std::lock_guard<std::mutex> lock(_mutex);
                    index = _requests.size();
                    _requests.push_back({request, connection});
                }
                const Reply reply = _script ? _script(request, index) : Reply(response());
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                if (!socket->writeBytes(reply.bytes,
                                        [deadline]
                                        { return std::chrono::steady_clock::now() >= deadline; }) ||
                    reply.close)
                {
                    break;
                }
            }
            state->setTerminated();
        }
        size_t getConnectedClientsCount() override
        {
            return 0;
        }
        Script _script;
        bool _tls;
        std::atomic<unsigned> _connections {0};
        std::mutex _mutex;
        std::vector<Request> _requests;
    };

    void configure(ix::HttpClient& client)
    {
#ifdef IX_HTTP_REUSE_TEST_CERTS
        ix::SocketTLSOptions tls;
        tls.caFile = std::string(IX_HTTP_REUSE_TEST_CERTS) + "/ca.pem";
        client.setTLSOptions(tls);
#else
        (void) client;
#endif
    }
    ix::HttpRequestArgsPtr options(ix::HttpClient& client)
    {
        auto args = client.createRequest();
        args->connectTimeout = 1;
        args->transferTimeout = 1;
        args->compress = false;
        return args;
    }

    // Deterministically fail after the idle probe: no timing-dependent FIN race.
    class StaleSocket final : public ix::Socket
    {
    public:
        explicit StaleSocket(std::string failure)
            : _failure(std::move(failure))
        {
        }
        std::ptrdiff_t recv(void* data, size_t) override
        {
            if (!_probed)
            {
                _probed = true;
                if (_failure == "closed")
                {
                    return 0;
                }
                if (_failure == "junk")
                {
                    *static_cast<char*>(data) = 'x';
                    return 1;
                }
                setErrno(EWOULDBLOCK);
                return -1;
            }
            const std::string bytes = _failure == "partial" ? "HTTP/1.1"
                                      : _failure == "informational"
                                          ? "HTTP/1.1 100 Continue\r\n\r\n"
                                          : "";
            if (_offset < bytes.size())
            {
                *static_cast<char*>(data) = bytes[_offset++];
                return 1;
            }
            return 0;
        }
        std::ptrdiff_t send(char*, size_t size) override
        {
            return _failure == "send" ? 0 : static_cast<std::ptrdiff_t>(size);
        }

    private:
        std::string _failure;
        bool _probed = false;
        size_t _offset = 0;
    };
} // namespace

TEST_CASE("HTTP connections are reused after complete length and chunked responses",
          "[http][reuse]")
{
#ifdef IX_HTTP_REUSE_TEST_CERTS
    const bool tls = GENERATE(false, true);
#else
    const bool tls = false;
#endif
    const bool chunked = GENERATE(false, true);
    const bool streamed = GENERATE(false, true);
    PersistentServer server(
        [&](const ix::HttpRequestPtr&, size_t)
        {
            return Reply(chunked
                             ? "HTTP/1.1 200 OK\r\nTransfer-Encoding: "
                               "chunked\r\n\r\n2\r\nhe\r\n3\r\nllo\r\n0\r\nX-Trailer: value\r\n\r\n"
                             : response("hello"));
        },
        tls);
    ix::HttpClient client;
    configure(client);
    auto args = options(client);
    for (int i = 0; i < 3; ++i)
    {
        std::string received;
        if (streamed)
        {
            args->onChunkCallback = [&](const std::string& bytes) { received += bytes; };
        }
        const auto result = client.get(server.url(), args);
        REQUIRE(result->errorCode == ix::HttpErrorCode::Ok);
        CHECK(result->downloadSize == 5);
        CHECK((streamed ? received : result->body) == "hello");
        args->onChunkCallback = nullptr;
    }
    const auto requests = server.requests();
    REQUIRE(requests.size() == 3);
    CHECK(requests[0].connection == requests[1].connection);
    CHECK(requests[1].connection == requests[2].connection);
}

TEST_CASE("HTTP Connection close tokens prevent reuse", "[http][reuse]")
{
    const std::string close = GENERATE("Connection: keep-alive, \tCLOSE\t\r\n",
                                       "Connection: close\r\nConnection: keep-alive\r\n",
                                       "request");
    PersistentServer server([&](const ix::HttpRequestPtr&, size_t)
                            { return Reply(response("ok", close == "request" ? "" : close)); });
    ix::HttpClient client;
    auto args = options(client);
    if (close == "request")
    {
        args->extraHeaders["Connection"] = "keep-alive, \tCLOSE";
    }
    REQUIRE(client.get(server.url(), args)->errorCode == ix::HttpErrorCode::Ok);
    REQUIRE(client.get(server.url(), args)->errorCode == ix::HttpErrorCode::Ok);
    const auto requests = server.requests();
    REQUIRE(requests.size() == 2);
    CHECK(requests[0].connection != requests[1].connection);
}

TEST_CASE("HTTP reuse is scoped to one origin and current connection options", "[http][reuse]")
{
    PersistentServer first;
    PersistentServer second;
    ix::HttpClient client;
    auto args = options(client);
    REQUIRE(client.get(first.url(), args)->errorCode == ix::HttpErrorCode::Ok);
    REQUIRE(client.get(second.url(), args)->errorCode == ix::HttpErrorCode::Ok);
    REQUIRE(client.get(first.url(), args)->errorCode == ix::HttpErrorCode::Ok);
    client.setTLSOptions({});
    REQUIRE(client.get(first.url(), args)->errorCode == ix::HttpErrorCode::Ok);
    client.setProxyOptions({});
    REQUIRE(client.get(first.url(), args)->errorCode == ix::HttpErrorCode::Ok);
    const auto requests = first.requests();
    REQUIRE(requests.size() == 4);
    for (size_t i = 1; i < requests.size(); ++i)
    {
        CHECK(requests[i - 1].connection != requests[i].connection);
    }
}

TEST_CASE("Bodyless and informational responses leave the next HTTP response aligned",
          "[http][reuse]")
{
    PersistentServer server(
        [](const ix::HttpRequestPtr& request, size_t index)
        {
            if (request->method == "HEAD")
            {
                return Reply("HTTP/1.1 200 OK\r\nContent-Length: 999\r\n\r\n");
            }
            if (index == 1)
            {
                return Reply("HTTP/1.1 204 No Content\r\n\r\n");
            }
            if (index == 2)
            {
                return Reply("HTTP/1.1 304 Not Modified\r\nContent-Length: 999\r\n\r\n");
            }
            return Reply(
                "HTTP/1.1 103 Early Hints\r\nLink: </asset>\r\n\r\nHTTP/1.1 100 Continue\r\n\r\n" +
                response());
        });
    ix::HttpClient client;
    auto args = options(client);
    REQUIRE(client.head(server.url(), args)->errorCode == ix::HttpErrorCode::Ok);
    CHECK(client.get(server.url(), args)->statusCode == 204);
    CHECK(client.get(server.url(), args)->statusCode == 304);
    const auto result = client.get(server.url(), args);
    REQUIRE(result->errorCode == ix::HttpErrorCode::Ok);
    CHECK(result->body == "ok");
    const auto requests = server.requests();
    REQUIRE(requests.size() == 4);
    for (const auto& request : requests)
    {
        CHECK(request.connection == requests[0].connection);
    }
}

TEST_CASE("Same-origin redirects consume their bodies before reusing HTTP", "[http][reuse]")
{
    const bool chunked = GENERATE(false, true);
    PersistentServer server(
        [&](const ix::HttpRequestPtr& request, size_t index)
        {
            if (index != 0)
            {
                return Reply(response("target"));
            }
            return Reply("HTTP/1.1 302 Found\r\nLocation: http://" + request->headers.at("Host") +
                         "/target\r\n" +
                         (chunked ? "Transfer-Encoding: chunked\r\n\r\n4\r\nskip\r\n0\r\n\r\n"
                                  : "Content-Length: 4\r\n\r\nskip"));
        });
    ix::HttpClient client;
    auto args = options(client);
    std::string received;
    args->onChunkCallback = [&](const std::string& data) { received += data; };
    const auto result = client.get(server.url(), args);
    REQUIRE(result->errorCode == ix::HttpErrorCode::Ok);
    CHECK(received == "target");
    const auto requests = server.requests();
    REQUIRE(requests.size() == 2);
    CHECK(requests[0].connection == requests[1].connection);
}

TEST_CASE("Malformed or ambiguous HTTP responses cannot become reusable", "[http][reuse]")
{
    const std::string wire = GENERATE(
        "HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Length: 0\r\n\r\nx",
        "HTTP/1.1 200 OK\r\nContent-Length: 9\r\n\r\nx",
        "HTTP/1.1 200 OK\r\n\r\nx",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nxBAD\r\n",
        "HTTP/1.1 101 Switching Protocols\r\n\r\n",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 999\r\n\r\n0\r\n\r\n");
    PersistentServer server([&](const ix::HttpRequestPtr&, size_t index)
                            { return index == 0 ? Reply(wire, true) : Reply(response()); });
    ix::HttpClient client;
    auto args = options(client);
    const auto first = client.get(server.url(), args);
    if (first->errorCode == ix::HttpErrorCode::Ok)
    {
        CHECK(first->headers.count("Transfer-Encoding") == 1);
    }
    REQUIRE(client.get(server.url(), args)->errorCode == ix::HttpErrorCode::Ok);
    const auto requests = server.requests();
    REQUIRE(requests.size() == 2);
    CHECK(requests[0].connection != requests[1].connection);
}

TEST_CASE("Only bodyless safe HTTP requests retry a stale connection once", "[http][reuse]")
{
    const std::string failure =
        GENERATE("send", "status", "partial", "informational", "closed", "junk");
    const bool post = GENERATE(false, true);
    PersistentServer server;
    ix::HttpClient client;
    ix::HttpClientReuseTestAccess::cache(
        client, std::unique_ptr<ix::Socket>(new StaleSocket(failure)), server.url());
    auto args = options(client);
    const auto result =
        client.request(server.url(), post ? "POST" : "GET", post ? "body" : "", args);
    if ((!post && (failure == "send" || failure == "status")) || failure == "closed" ||
        failure == "junk")
    {
        CHECK(result->errorCode == ix::HttpErrorCode::Ok);
        CHECK(server.requests().size() == 1);
    }
    else
    {
        CHECK(result->errorCode != ix::HttpErrorCode::Ok);
        CHECK(server.requests().empty());
    }
    // A failed transfer must not poison the next request.
    REQUIRE(client.get(server.url(), options(client))->errorCode == ix::HttpErrorCode::Ok);
}

TEST_CASE("Cancellation and callback configuration changes discard HTTP connections",
          "[http][reuse]")
{
    const bool cancel = GENERATE(false, true);
    PersistentServer server(
        [](const ix::HttpRequestPtr&, size_t index)
        { return Reply(response(index == 0 ? std::string(32768, 'x') : "ok")); });
    ix::HttpClient client;
    auto args = options(client);
    args->onChunkCallback = [&](const std::string&)
    {
        if (cancel)
        {
            args->cancel = true;
        }
        else
        {
            client.setTLSOptions({});
        }
    };
    const auto first = client.get(server.url(), args);
    CHECK(first->errorCode == (cancel ? ix::HttpErrorCode::Cancelled : ix::HttpErrorCode::Ok));
    REQUIRE(client.get(server.url(), options(client))->errorCode == ix::HttpErrorCode::Ok);
    const auto requests = server.requests();
    REQUIRE(requests.size() == 2);
    CHECK(requests[0].connection != requests[1].connection);
}

TEST_CASE("Queued HTTP requests reuse their completed connection", "[http][reuse]")
{
    PersistentServer server;
    ix::HttpClient client(true);
    std::promise<ix::HttpResponsePtr> first;
    std::promise<ix::HttpResponsePtr> second;
    auto one = first.get_future();
    auto two = second.get_future();
    auto args = options(client);
    args->url = server.url();
    args->verb = "GET";
    REQUIRE(client.performRequest(
        args, [&](const ix::HttpResponsePtr& result) { first.set_value(result); }));
    REQUIRE(client.performRequest(
        args, [&](const ix::HttpResponsePtr& result) { second.set_value(result); }));
    REQUIRE(one.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    REQUIRE(two.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    CHECK(one.get()->errorCode == ix::HttpErrorCode::Ok);
    CHECK(two.get()->errorCode == ix::HttpErrorCode::Ok);
    const auto requests = server.requests();
    REQUIRE(requests.size() == 2);
    CHECK(requests[0].connection == requests[1].connection);
}

TEST_CASE("Streamed uploads can redirect over a reused HTTP connection", "[http][reuse][upload]")
{
    const int status = GENERATE(303, 307);
    PersistentServer server(
        [&](const ix::HttpRequestPtr& request, size_t index)
        {
            if (index == 0)
            {
                return Reply("HTTP/1.1 " + std::to_string(status) +
                             " Redirect\r\nLocation: http://" + request->headers.at("Host") +
                             "/target\r\nContent-Length: 4\r\n\r\nskip");
            }
            return Reply(response(request->body));
        });
    ix::HttpClient client;
    auto args = options(client);
    args->bodySize = 5;
    unsigned reads = 0;
    args->onUploadCallback = [&](uint64_t offset, char* buffer, size_t capacity)
    {
        ++reads;
        const std::string body = "hello";
        const size_t size = (std::min) (capacity, body.size() - static_cast<size_t>(offset));
        std::memcpy(buffer, body.data() + offset, size);
        return size;
    };
    const auto result = client.post(server.url(), "", args);
    REQUIRE(result->errorCode == ix::HttpErrorCode::Ok);
    CHECK(result->body == (status == 303 ? "" : "hello"));
    CHECK(reads == (status == 303 ? 1 : 2));
    CHECK(static_cast<bool>(args->onUploadCallback));
    const auto requests = server.requests();
    REQUIRE(requests.size() == 2);
    CHECK(requests[0].connection == requests[1].connection);
    CHECK(requests[1].http->method == (status == 303 ? "GET" : "POST"));
}

TEST_CASE("A fresh HTTP connection is not retried after a stale-connection retry", "[http][reuse]")
{
    PersistentServer server([](const ix::HttpRequestPtr&, size_t) { return Reply("", true); });
    ix::HttpClient client;
    ix::HttpClientReuseTestAccess::cache(
        client, std::unique_ptr<ix::Socket>(new StaleSocket("status")), server.url());
    const auto result = client.get(server.url(), options(client));
    CHECK(result->errorCode == ix::HttpErrorCode::CannotReadStatusLine);
    CHECK(server.requests().size() == 1);
}

#ifdef IXWEBSOCKET_USE_ZLIB
TEST_CASE("Gzip HTTP responses can reuse their fully consumed connection", "[http][reuse]")
{
    const auto encoded = ix::gzipCompress("hello");
    REQUIRE_FALSE(encoded.empty());
    PersistentServer server([&](const ix::HttpRequestPtr&, size_t)
                            { return Reply(response(encoded, "Content-Encoding: gzip\r\n")); });
    ix::HttpClient client;
    auto args = options(client);
    for (int i = 0; i < 2; ++i)
    {
        const auto result = client.get(server.url(), args);
        REQUIRE(result->errorCode == ix::HttpErrorCode::Ok);
        CHECK(result->body == "hello");
    }
    const auto requests = server.requests();
    REQUIRE(requests.size() == 2);
    CHECK(requests[0].connection == requests[1].connection);
}
#endif

#ifdef IX_HTTP_REUSE_TEST_CERTS
TEST_CASE("Changing HTTP trust options cannot reuse the previous authenticated TLS socket",
          "[http][reuse][tls]")
{
    PersistentServer server({}, true);
    ix::HttpClient client;
    configure(client);
    auto args = options(client);
    REQUIRE(client.get(server.url(), args)->errorCode == ix::HttpErrorCode::Ok);
    ix::SocketTLSOptions untrusted;
    untrusted.caFile = std::string(IX_HTTP_REUSE_TEST_CERTS) + "/untrusted.pem";
    client.setTLSOptions(untrusted);
    CHECK(client.get(server.url(), args)->errorCode == ix::HttpErrorCode::CannotConnect);
    CHECK(server.requests().size() == 1);
    configure(client);
    REQUIRE(client.get(server.url(), args)->errorCode == ix::HttpErrorCode::Ok);
    const auto requests = server.requests();
    REQUIRE(requests.size() == 2);
    CHECK(requests[0].connection != requests[1].connection);
}
#endif
