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
#include <limits>
#include <mutex>
#include <sstream>
#include <stdexcept>

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

    class ChunkedUploadServer final : public ix::SocketServer
    {
    public:
        struct Request
        {
            std::string method, body, wire, connection;
            ix::WebSocketHttpHeaders headers;
        };

        explicit ChunkedUploadServer(bool tls = false, int redirect = 0)
            : SocketServer(ix::getFreePort(), "127.0.0.1")
            , _tls(tls)
            , _redirect(redirect)
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

        ~ChunkedUploadServer() override
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

        std::atomic<unsigned> connections {0};

    private:
        size_t getConnectedClientsCount() override
        {
            return 0;
        }

        void handleConnection(std::unique_ptr<ix::Socket> socket,
                              std::shared_ptr<ix::ConnectionState> state) override
        {
            ++connections;
            while (true)
            {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                const auto cancelled = [deadline]
                { return std::chrono::steady_clock::now() >= deadline; };
                const auto line = socket->readLine(cancelled);
                if (!line.first)
                {
                    break;
                }
                const auto parsed = ix::parseHttpHeaders(socket, cancelled);
                if (!parsed.first)
                {
                    break;
                }
                Request request;
                request.method = std::get<0>(ix::Http::parseRequestLine(line.second));
                request.headers = parsed.second;
                request.connection = state->getId();
                bool complete = true;
                if (request.headers.count("Transfer-Encoding"))
                {
                    while (true)
                    {
                        const auto sizeLine = socket->readLine(cancelled);
                        if (!sizeLine.first)
                        {
                            complete = false;
                            break;
                        }
                        request.wire += sizeLine.second;
                        size_t size = 0;
                        std::istringstream input(sizeLine.second);
                        if (!(input >> std::hex >> size))
                        {
                            complete = false;
                            break;
                        }
                        if (size == 0)
                        {
                            const auto end = socket->readLine(cancelled);
                            complete = end.first && end.second == "\r\n";
                            request.wire += end.second;
                            break;
                        }
                        const auto data = socket->readBytes(size, nullptr, nullptr, cancelled);
                        const auto end = socket->readLine(cancelled);
                        if (!data.first || !end.first || end.second != "\r\n")
                        {
                            complete = false;
                            break;
                        }
                        request.body += data.second;
                        request.wire += data.second + end.second;
                    }
                }
                if (!complete)
                {
                    break;
                }
                size_t index;
                {
                    std::lock_guard<std::mutex> lock(_mutex);
                    index = _requests.size();
                    _requests.push_back(request);
                }
                if (_redirect && index == 0)
                {
                    const std::string response = "HTTP/1.1 " + std::to_string(_redirect) +
                                                 " Redirect\r\nLocation: " + url() +
                                                 "target\r\nContent-Length: 4\r\n\r\nskip";
                    if (!socket->writeBytes(response, cancelled))
                    {
                        break;
                    }
                }
                else if (!socket->writeBytes("HTTP/1.1 200 OK\r\nContent-Length: " +
                                                 std::to_string(request.body.size()) + "\r\n\r\n" +
                                                 request.body,
                                             cancelled))
                {
                    break;
                }
            }
            state->setTerminated();
        }

        bool _tls;
        int _redirect;
        std::mutex _mutex;
        std::vector<Request> _requests;
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
        args->extraHeaders["transfer-encoding"] = "gzip";
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

TEST_CASE("HTTP chunked uploads frame buffered and unknown-length binary bodies",
          "[http][upload][chunked]")
{
    const bool streamed = GENERATE(false, true);
    const std::string method = GENERATE("POST", "PUT", "PATCH");
    const size_t size = GENERATE(size_t(0), size_t(7), size_t(70000));
#ifdef IX_HTTP_UPLOAD_TEST_CERTS
    const bool tls = GENERATE(false, true);
#else
    const bool tls = false;
#endif
    ChunkedUploadServer server(tls);
    ix::HttpClient client;
    auto args = options(client);
    args->extraHeaders["transfer-encoding"] = " \tChUnKeD ";
    std::string expected(size, 'x');
    if (size)
    {
        expected[size / 2] = '\0';
    }
    unsigned eof = 0;
    if (streamed)
    {
        args->bodySize = (std::numeric_limits<uint64_t>::max)(); // Ignored in chunked mode.
        args->onUploadCallback = [&](uint64_t offset, char* output, size_t capacity)
        {
            CHECK(capacity <= 65536);
            if (offset == expected.size())
            {
                ++eof;
                return size_t(0);
            }
            const size_t count =
                (std::min) (size_t(7000), expected.size() - static_cast<size_t>(offset));
            std::memcpy(output, expected.data() + offset, count);
            return count;
        };
    }
    const auto response = client.request(server.url(), method, streamed ? "" : expected, args);
    INFO(response->errorMsg);
    REQUIRE(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(response->body == expected);
    CHECK(eof == (streamed ? 1 : 0));
    auto requests = server.requests();
    REQUIRE(requests.size() == 1);
    CHECK(requests[0].headers.count("Content-Length") == 0);
    CHECK(requests[0].body == expected);
    REQUIRE(requests[0].wire.size() >= 5);
    CHECK(requests[0].wire.substr(requests[0].wire.size() - 5) == "0\r\n\r\n");
    CHECK(response->uploadSize > requests[0].wire.size());
    args->onUploadCallback = nullptr;
    args->extraHeaders.clear();
    REQUIRE(client.get(server.url(), args)->errorCode == ix::HttpErrorCode::Ok);
    CHECK(server.connections == 1); // The terminator left the connection aligned.
}

TEST_CASE("Chunked HTTP redirects replay uploads or switch to GET", "[http][upload][chunked]")
{
    const int status = GENERATE(303, 307, 308);
    ChunkedUploadServer server(false, status);
    ix::HttpClient client;
    auto args = options(client);
    args->extraHeaders["Transfer-Encoding"] = "chunked";
    unsigned starts = 0;
    args->onUploadCallback = [&](uint64_t offset, char* output, size_t)
    {
        if (offset)
        {
            return size_t(0);
        }
        ++starts;
        std::memcpy(output, "hello", 5);
        return size_t(5);
    };
    const auto response = client.post(server.url(), "", args);
    INFO(response->errorMsg);
    REQUIRE(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(starts == (status == 303 ? 1 : 2));
    CHECK(response->body == (status == 303 ? "" : "hello"));
    const auto requests = server.requests();
    REQUIRE(requests.size() == 2);
    CHECK(requests[1].method == (status == 303 ? "GET" : "POST"));
    CHECK(requests[1].headers.count("Transfer-Encoding") == (status == 303 ? 0 : 1));
    CHECK(server.connections == 1);
}

TEST_CASE("Chunked HTTP failures cancel and discard the connection", "[http][upload][chunked]")
{
    const int failure = GENERATE(0, 1, 2);
    ChunkedUploadServer server;
    ix::HttpClient client;
    auto args = options(client);
    args->extraHeaders["Transfer-Encoding"] = "chunked";
    args->onUploadCallback = [&](uint64_t, char*, size_t capacity) -> size_t
    {
        if (failure == 0)
        {
            throw std::runtime_error("source failed");
        }
        if (failure == 1)
        {
            return capacity + 1;
        }
        args->cancel = true;
        return 0;
    };
    const auto response = client.post(server.url(), "", args);
    CHECK(response->errorCode ==
          (failure == 2 ? ix::HttpErrorCode::Cancelled : ix::HttpErrorCode::SendError));
    args->cancel = false;
    args->onUploadCallback = nullptr;
    args->extraHeaders.clear();
    REQUIRE(client.get(server.url(), args)->errorCode == ix::HttpErrorCode::Ok);
    CHECK(server.connections == 2);
}

TEST_CASE("Chunked HTTP uploads reject ambiguous or unsupported framing", "[http][upload][chunked]")
{
    const std::string coding = GENERATE("chunked", "gzip", "gzip, chunked", "chunked, chunked", "");
    ix::HttpClient client;
    auto args = options(client);
    args->extraHeaders["Transfer-Encoding"] = coding;
    if (coding == "chunked")
    {
        args->extraHeaders["Content-Length"] = "0";
    }
    CHECK(client.post("http://127.0.0.1:1/", "body", args)->errorCode ==
          ix::HttpErrorCode::Invalid);
    args->extraHeaders.clear();
    args->extraHeaders["Transfer-Encoding"] = "chunked";
    CHECK(client.get("http://127.0.0.1:1/", args)->errorCode == ix::HttpErrorCode::Invalid);
    args->compressRequest = true;
    CHECK(client.post("http://127.0.0.1:1/", "body", args)->errorCode ==
          ix::HttpErrorCode::Invalid);
}

TEST_CASE("Asynchronous chunked uploads distinguish response chunks from completion",
          "[http][upload][chunked]")
{
    ChunkedUploadServer server;
    std::promise<ix::HttpResponsePtr> completed;
    auto result = completed.get_future();
    std::string received;
    ix::HttpClient client(true);
    auto args = options(client);
    args->url = server.url();
    args->verb = "POST";
    args->extraHeaders["Transfer-Encoding"] = "chunked";
    args->onUploadCallback = [](uint64_t offset, char* output, size_t)
    {
        if (offset)
        {
            return size_t(0);
        }
        std::memcpy(output, "hello", 5);
        return size_t(5);
    };
    args->onChunkCallback = [&](const std::string& data) { received += data; };
    REQUIRE(client.performRequest(
        args, [&](const ix::HttpResponsePtr& response) { completed.set_value(response); }));
    REQUIRE(result.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    const auto response = result.get();
    REQUIRE(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(response->body.empty());
    CHECK(received == "hello");
}
