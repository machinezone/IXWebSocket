/*
 *  IXHttpClientUploadTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <catch_amalgamated.hpp>
#include <cstring>
#include <future>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXHttpServer.h>
#include <ixwebsocket/IXSocketServer.h>

namespace
{
    class UploadServer final : public ix::SocketServer
    {
        std::promise<ix::WebSocketHttpHeaders> _headers;

    public:
        explicit UploadServer(bool headersOnly = false, bool redirect = false, bool tls = false)
            : SocketServer(ix::getFreePort(), "127.0.0.1")
            , _headersOnly(headersOnly)
            , _redirect(redirect)
            , _tls(tls)
        {
#ifdef IX_HTTP_UPLOAD_TEST_CERTS
            if (tls)
            {
                ix::SocketTLSOptions options;
                options.tls = true;
                options.certFile = std::string(IX_HTTP_UPLOAD_TEST_CERTS) + "/server.pem";
                options.keyFile = std::string(IX_HTTP_UPLOAD_TEST_CERTS) + "/server.key";
                options.caFile = "NONE";
                setTLSOptions(options);
            }
#endif
            REQUIRE(listen().first);
            start();
        }
        ~UploadServer() override
        {
            stop();
        }
        std::string url()
        {
            return std::string(_tls ? "https://127.0.0.1:" : "http://127.0.0.1:") +
                   std::to_string(getPort()) + "/";
        }
        std::future<ix::WebSocketHttpHeaders> headers = _headers.get_future();

    private:
        size_t getConnectedClientsCount() override
        {
            return 0;
        }
        void handleConnection(std::unique_ptr<ix::Socket> socket,
                              std::shared_ptr<ix::ConnectionState> state) override
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            const auto cancelled = [deadline]
            { return std::chrono::steady_clock::now() >= deadline; };
            if (_headersOnly)
            {
                socket->readLine(cancelled);
                const auto parsed = ix::parseHttpHeaders(socket, cancelled);
                _headers.set_value(parsed.second);
            }
            else
            {
                const auto parsed = ix::Http::parseRequest(socket, 2);
                if (std::get<0>(parsed))
                {
                    const auto request = std::get<2>(parsed);
                    auto response = std::make_shared<ix::HttpResponse>(200,
                                                                       "OK",
                                                                       ix::HttpErrorCode::Ok,
                                                                       ix::WebSocketHttpHeaders(),
                                                                       request->body);
                    if (_redirect && request->uri == "/")
                    {
                        response->statusCode = 307;
                        response->headers["Location"] = url() + "target";
                        response->body.clear();
                    }
                    ix::Http::sendResponse(response, socket, true);
                }
            }
            state->setTerminated();
        }
        const bool _headersOnly;
        const bool _redirect;
        const bool _tls;
    };

    ix::HttpRequestArgsPtr options(ix::HttpClient& client)
    {
#ifdef IX_HTTP_UPLOAD_TEST_CERTS
        ix::SocketTLSOptions tls;
        tls.caFile = std::string(IX_HTTP_UPLOAD_TEST_CERTS) + "/ca.pem";
        client.setTLSOptions(tls);
#endif
        auto args = client.createRequest();
        args->connectTimeout = 1;
        args->transferTimeout = 1;
        args->compress = false;
        return args;
    }
} // namespace

TEST_CASE("HTTP uploads stream bounded chunks and replay after redirects", "[http][upload]")
{
    const bool redirect = GENERATE(false, true);
    const std::string method =
        GENERATE(std::string("POST"), std::string("PUT"), std::string("PATCH"));
#ifdef IX_HTTP_UPLOAD_TEST_CERTS
    const bool tls = GENERATE(false, true);
#else
    const bool tls = false;
#endif
    UploadServer server(false, redirect, tls);
    ix::HttpClient client;
    auto args = options(client);
    std::string expected(200000, 'x');
    expected[65535] = '\0';
    expected.back() = 'y';
    args->bodySize = expected.size();
    size_t starts = 0;
    size_t maxCapacity = 0;
    args->onUploadCallback = [&](uint64_t offset, char* buffer, size_t capacity)
    {
        if (offset == 0)
        {
            ++starts;
        }
        maxCapacity = (std::max) (maxCapacity, capacity);
        // A source may produce fewer bytes than requested, without reaching EOF.
        const size_t count = (std::min) (size_t(7000), capacity);
        std::memcpy(buffer, expected.data() + static_cast<size_t>(offset), count);
        return count;
    };
    auto response = client.request(server.url(), method, std::string(), args);
    INFO(response->errorMsg);
    REQUIRE(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(response->body == expected);
    CHECK(starts == (redirect ? 2 : 1));
    CHECK(maxCapacity <= 64 * 1024);
    CHECK(response->uploadSize > expected.size());
}

TEST_CASE("HTTP streamed uploads retain 64-bit declared lengths", "[http][upload]")
{
    UploadServer server(true);
    ix::HttpClient client;
    auto args = options(client);
    args->bodySize = (uint64_t(1) << 32) + 123;
    size_t capacity = 0;
    args->onUploadCallback = [&](uint64_t offset, char*, size_t requested)
    {
        CHECK(offset == 0);
        capacity = requested;
        return size_t(0); // No giant file or allocation is needed to verify framing.
    };
    auto response = client.put(server.url(), std::string(), args);
    CHECK(response->errorCode == ix::HttpErrorCode::SendError);
    CHECK(capacity == 64 * 1024);
    REQUIRE(server.headers.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    CHECK(server.headers.get().at("Content-Length") == std::to_string(args->bodySize));
}

TEST_CASE("HTTP streamed upload failures and cancellation stop the source", "[http][upload]")
{
    const int failure = GENERATE(0, 1, 2, 3);
    UploadServer server(true);
    ix::HttpClient client;
    auto args = options(client);
    args->bodySize = 5;
    size_t calls = 0;
    args->onUploadCallback = [&](uint64_t, char* buffer, size_t capacity) -> size_t
    {
        ++calls;
        if (failure == 0)
        {
            return 0;
        }
        if (failure == 1)
        {
            return capacity + 1;
        }
        if (failure == 2)
        {
            throw std::runtime_error("read failed");
        }
        args->cancel = true;
        buffer[0] = 'x';
        return 1;
    };
    auto response = client.post(server.url(), std::string(), args);
    CHECK(response->errorCode ==
          (failure == 3 ? ix::HttpErrorCode::Cancelled : ix::HttpErrorCode::SendError));
    CHECK(calls == 1);
}

TEST_CASE("HTTP streamed uploads reject conflicting request options", "[http][upload]")
{
    const int conflict = GENERATE(0, 1, 2, 3, 4);
    ix::HttpClient client;
    auto args = options(client);
    args->onUploadCallback = [](uint64_t, char*, size_t) { return size_t(0); };
    if (conflict == 0)
    {
        args->compressRequest = true;
    }
    if (conflict == 1)
    {
        args->extraHeaders["content-length"] = "0";
    }
    if (conflict == 2)
    {
        args->extraHeaders["transfer-encoding"] = "chunked";
    }
    const auto response = client.request(
        "http://127.0.0.1:1", conflict == 3 ? "GET" : "POST", conflict == 4 ? "body" : "", args);
    CHECK(response->errorCode == ix::HttpErrorCode::Invalid);
}

TEST_CASE("Empty HTTP streamed uploads do not read the source", "[http][upload]")
{
    UploadServer server;
    ix::HttpClient client;
    auto args = options(client);
    size_t calls = 0;
    args->onUploadCallback = [&](uint64_t, char*, size_t)
    {
        ++calls;
        return size_t(0);
    };
    auto response = client.put(server.url(), std::string(), args);
    CHECK(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(response->body.empty());
    CHECK(calls == 0);
}
