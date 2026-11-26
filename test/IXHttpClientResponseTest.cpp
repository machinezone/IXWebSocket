/*
 *  IXHttpClientResponseTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <catch_amalgamated.hpp>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXSocketServer.h>
#include <ixwebsocket/IXWebSocket.h>
#include <mutex>

namespace
{
    class ResponseServer final : public ix::SocketServer
    {
    public:
        using Script = std::function<std::string(const ix::HttpRequestPtr&, size_t)>;

        explicit ResponseServer(Script script)
            : SocketServer(ix::getFreePort(), "127.0.0.1")
            , _script(std::move(script))
        {
        }

        ~ResponseServer() override
        {
            stop();
        }

        void startServing()
        {
            REQUIRE(listen().first);
            start();
        }

        std::string url()
        {
            return "http://127.0.0.1:" + std::to_string(getPort()) + "/";
        }

        std::vector<ix::HttpRequestPtr> requests()
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _requests;
        }

    private:
        void handleConnection(std::unique_ptr<ix::Socket> socket,
                              std::shared_ptr<ix::ConnectionState> state) override
        {
            const auto request = ix::Http::parseRequest(socket, 2);
            if (std::get<0>(request))
            {
                const auto parsed = std::get<2>(request);
                size_t index;
                {
                    std::lock_guard<std::mutex> lock(_mutex);
                    index = _requests.size();
                    _requests.push_back(parsed);
                }
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                auto response = _script(parsed, index);
                const auto end = response.find("\r\n");
                if (end != std::string::npos)
                {
                    // This fixture serves one request per connection.
                    response.insert(end + 2, "Connection: close\r\n");
                }
                socket->writeBytes(
                    response, [deadline] { return std::chrono::steady_clock::now() >= deadline; });
            }
            state->setTerminated();
        }

        size_t getConnectedClientsCount() override
        {
            return 0;
        }

        Script _script;
        std::mutex _mutex;
        std::vector<ix::HttpRequestPtr> _requests;
    };

    ix::HttpRequestArgsPtr options(ix::HttpClient& client)
    {
        auto args = client.createRequest();
        args->connectTimeout = 1;
        args->transferTimeout = 1;
        args->compress = false;
        return args;
    }
} // namespace

TEST_CASE("Bodyless HTTP responses do not read representation bodies", "[http][response]")
{
    const int status = GENERATE(204, 304);
    ResponseServer server(
        [status](const ix::HttpRequestPtr&, size_t)
        {
            return "HTTP/1.1 " + std::to_string(status) + " Status\r\nContent-Length: 123\r\n\r\n";
        });
    server.startServing();
    ix::HttpClient client;
    auto response = client.get(server.url(), options(client));
    CHECK(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(response->statusCode == status);
    CHECK(response->body.empty());
}

TEST_CASE("Only followable redirect statuses are followed", "[http][response]")
{
    const int status = GENERATE(300, 305, 306);
    std::string redirectUrl;
    ResponseServer server(
        [&](const ix::HttpRequestPtr&, size_t)
        {
            return "HTTP/1.1 " + std::to_string(status) + " Status\r\nLocation: " + redirectUrl +
                   "\r\nContent-Length: 0\r\n\r\n";
        });
    redirectUrl = server.url();
    server.startServing();
    ix::HttpClient client;
    auto args = options(client);
    args->maxRedirects = 1;
    auto response = client.get(server.url(), args);
    CHECK(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(response->statusCode == status);
    CHECK(server.requests().size() == 1);
}

TEST_CASE("303 redirects retrieve the target without replaying the body", "[http][response]")
{
    const std::string verb = GENERATE(std::string("POST"), std::string("PUT"), std::string("HEAD"));
    std::string redirectUrl;
    ResponseServer server(
        [&](const ix::HttpRequestPtr&, size_t index)
        {
            return index == 0 ? "HTTP/1.1 303 See Other\r\nLocation: " + redirectUrl +
                                    "\r\nContent-Length: 0\r\n\r\n"
                              : "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
        });
    redirectUrl = server.url();
    server.startServing();
    ix::HttpClient client;
    auto args = options(client);
    args->extraHeaders["Content-Type"] = "application/custom";
    auto response = client.request(server.url(), verb, "body", args);
    REQUIRE(response->errorCode == ix::HttpErrorCode::Ok);
    auto requests = server.requests();
    REQUIRE(requests.size() == 2);
    CHECK(requests[1]->method == (verb == "HEAD" ? "HEAD" : "GET"));
    CHECK(requests[1]->body.empty());
    if (verb != "HEAD")
    {
        CHECK(requests[1]->headers.count("Content-Type") == 0);
    }
    CHECK(args->extraHeaders.at("Content-Type") == "application/custom");
}

TEST_CASE("307 and 308 redirects preserve the request method and body", "[http][response]")
{
    const int status = GENERATE(307, 308);
    std::string redirectUrl;
    ResponseServer server(
        [&](const ix::HttpRequestPtr&, size_t index)
        {
            return index == 0 ? "HTTP/1.1 " + std::to_string(status) +
                                    " Redirect\r\nLocation: " + redirectUrl +
                                    "\r\nContent-Length: 0\r\n\r\n"
                              : "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
        });
    redirectUrl = server.url();
    server.startServing();
    ix::HttpClient client;
    auto response = client.request(server.url(), "POST", "body", options(client));
    REQUIRE(response->errorCode == ix::HttpErrorCode::Ok);
    auto requests = server.requests();
    REQUIRE(requests.size() == 2);
    CHECK(requests[1]->method == "POST");
    CHECK(requests[1]->body == "body");
}

TEST_CASE("HTTP redirects keep credentials within the original origin",
          "[http][redirect_credentials]")
{
    const bool crossOrigin = GENERATE(false, true);
    const int status = GENERATE(301, 302, 303, 307, 308);
    const std::string ok = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    ResponseServer target([&](const ix::HttpRequestPtr&, size_t) { return ok; });
    target.startServing();
    std::string location;
    ResponseServer source(
        [&](const ix::HttpRequestPtr&, size_t index)
        {
            return index == 0
                       ? "HTTP/1.1 " + std::to_string(status) +
                             " Redirect\r\nLocation: " + location + "\r\nContent-Length: 0\r\n\r\n"
                       : ok;
        });
    location = crossOrigin ? target.url() : source.url();
    source.startServing();
    ix::HttpClient client;
    auto args = options(client);
    args->extraHeaders["authorization"] = "Bearer secret";
    args->extraHeaders["cOoKiE"] = "session=secret";
    args->extraHeaders["X-Trace"] = "trace";
    auto response = client.request(source.url(), "POST", "body", args);
    REQUIRE(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(response->statusCode == 200);
    const auto initial = source.requests();
    REQUIRE(initial.size() == (crossOrigin ? 1 : 2));
    CHECK(initial[0]->headers.at("Authorization") == "Bearer secret");
    CHECK(initial[0]->headers.at("Cookie") == "session=secret");
    const auto redirected = crossOrigin ? target.requests() : initial;
    REQUIRE_FALSE(redirected.empty());
    CHECK(redirected.back()->headers.count("Authorization") == (crossOrigin ? 0 : 1));
    CHECK(redirected.back()->headers.count("Cookie") == (crossOrigin ? 0 : 1));
    CHECK(redirected.back()->headers.at("X-Trace") == "trace");
    CHECK(args->extraHeaders.at("Authorization") == "Bearer secret");
    CHECK(args->extraHeaders.at("Cookie") == "session=secret");
}

TEST_CASE("WebSocket redirects keep credentials within the original origin",
          "[websocket][redirect_credentials]")
{
    const bool crossOrigin = GENERATE(false, true);
    const std::string rejected = "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n";
    ResponseServer target([&](const ix::HttpRequestPtr&, size_t) { return rejected; });
    target.startServing();
    std::string location;
    ResponseServer source(
        [&](const ix::HttpRequestPtr&, size_t index)
        {
            return index == 0 ? "HTTP/1.1 302 Found\r\nLocation: " + location +
                                    "\r\nContent-Length: 0\r\n\r\n"
                              : rejected;
        });
    std::string originalUrl = source.url();
    originalUrl.replace(0, 4, "ws");
    location = crossOrigin ? target.url() : source.url();
    location.replace(0, 4, "ws");
    source.startServing();
    ix::WebSocket client;
    client.setUrl(originalUrl);
    const ix::WebSocketHttpHeaders headers = {
        {"authorization", "Bearer secret"}, {"cOoKiE", "session=secret"}, {"X-Trace", "trace"}};
    client.setExtraHeaders(headers);
    auto result = client.connect(1);
    CHECK_FALSE(result.success);
    CHECK(result.http_status == 401);
    const auto initial = source.requests();
    REQUIRE(initial.size() == (crossOrigin ? 1 : 2));
    CHECK(initial[0]->headers.at("Authorization") == "Bearer secret");
    CHECK(initial[0]->headers.at("Cookie") == "session=secret");
    const auto redirected = crossOrigin ? target.requests() : initial;
    REQUIRE_FALSE(redirected.empty());
    CHECK(redirected.back()->headers.count("Authorization") == (crossOrigin ? 0 : 1));
    CHECK(redirected.back()->headers.count("Cookie") == (crossOrigin ? 0 : 1));
    CHECK(redirected.back()->headers.at("X-Trace") == "trace");
    CHECK(client.connect(1).http_status == 401);
    const auto retried = source.requests();
    CHECK(retried.back()->headers.at("Authorization") == "Bearer secret");
    CHECK(retried.back()->headers.at("Cookie") == "session=secret");
}

TEST_CASE("Returning to the original origin does not restore redirect credentials",
          "[http][redirect_credentials]")
{
    const bool websocket = GENERATE(false, true);
    std::string originalUrl;
    ResponseServer target(
        [&](const ix::HttpRequestPtr&, size_t)
        {
            return "HTTP/1.1 302 Found\r\nLocation: " + originalUrl +
                   "\r\nContent-Length: 0\r\n\r\n";
        });
    REQUIRE(target.listen().first);
    std::string targetUrl = target.url();
    if (websocket)
    {
        targetUrl.replace(0, 4, "ws");
    }
    ResponseServer source(
        [&](const ix::HttpRequestPtr&, size_t index)
        {
            return index == 0 ? "HTTP/1.1 302 Found\r\nLocation: " + targetUrl +
                                    "\r\nContent-Length: 0\r\n\r\n"
                              : "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
        });
    originalUrl = source.url();
    if (websocket)
    {
        originalUrl.replace(0, 4, "ws");
    }
    REQUIRE(source.listen().first);
    target.start();
    source.start();
    ix::HttpClient client;
    auto args = options(client);
    args->extraHeaders["Authorization"] = "Bearer secret";
    args->extraHeaders["Cookie"] = "session=secret";
    if (websocket)
    {
        ix::WebSocket webSocket;
        webSocket.setUrl(originalUrl);
        webSocket.setExtraHeaders(args->extraHeaders);
        CHECK(webSocket.connect(1).http_status == 200);
    }
    else
    {
        REQUIRE(client.get(originalUrl, args)->errorCode == ix::HttpErrorCode::Ok);
    }
    const auto requests = source.requests();
    REQUIRE(requests.size() == 2);
    CHECK(requests[0]->headers.count("Authorization") == 1);
    CHECK(requests[0]->headers.count("Cookie") == 1);
    CHECK(requests[1]->headers.count("Authorization") == 0);
    CHECK(requests[1]->headers.count("Cookie") == 0);
    const auto targetRequests = target.requests();
    REQUIRE(targetRequests.size() == 1);
    CHECK(targetRequests[0]->headers.count("Authorization") == 0);
    CHECK(targetRequests[0]->headers.count("Cookie") == 0);
}

TEST_CASE("Invalid HTTP content lengths are rejected", "[http][response]")
{
    const std::string length = GENERATE(std::string("-1"),
                                        std::string("+1"),
                                        std::string("1x"),
                                        std::string(""),
                                        std::string("999999999999999999999999"));
    ResponseServer server([&](const ix::HttpRequestPtr&, size_t)
                          { return "HTTP/1.1 200 OK\r\nContent-Length: " + length + "\r\n\r\nZ"; });
    server.startServing();
    ix::HttpClient client;
    auto response = client.get(server.url(), options(client));
    CAPTURE(length, response->errorMsg);
    CHECK(response->errorCode == ix::HttpErrorCode::HeaderParsingError);
}

TEST_CASE("Chunked responses handle case, extensions, trailers and length precedence",
          "[http][response]")
{
    const std::string coding = GENERATE(std::string("chunked"), std::string("ChUnKeD"));
    const bool hasLength = GENERATE(false, true);
    const bool streaming = GENERATE(false, true);
    ResponseServer server(
        [&](const ix::HttpRequestPtr&, size_t)
        {
            return "HTTP/1.1 200 OK\r\nTransfer-Encoding: " + coding + "\r\n" +
                   (hasLength ? "Content-Length: 999\r\n" : "") +
                   "\r\n3;extension=value\r\nabc\r\n2\r\nde\r\n0\r\nX-Trailer: value\r\n\r\n";
        });
    server.startServing();
    ix::HttpClient client;
    auto args = options(client);
    std::string received;
    if (streaming)
    {
        args->onChunkCallback = [&](const std::string& bytes) { received += bytes; };
    }
    auto response = client.get(server.url(), args);
    CAPTURE(response->errorMsg);
    CHECK(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK((streaming ? received : response->body) == "abcde");
    CHECK(response->downloadSize == 5);
}

TEST_CASE("Malformed HTTP chunks fail instead of accepting partial messages", "[http][response]")
{
    const std::string body = GENERATE(std::string("G\r\n\r\n"),
                                      std::string("1x\r\nZ\r\n0\r\n\r\n"),
                                      std::string("-1\r\n"),
                                      std::string("FFFFFFFFFFFFFFFFFFFFFFFF\r\n"),
                                      std::string("1\r\nZinvalid-terminator\r\n0\r\n\r\n"));
    ResponseServer server(
        [&](const ix::HttpRequestPtr&, size_t)
        { return "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n" + body; });
    server.startServing();
    ix::HttpClient client;
    auto response = client.get(server.url(), options(client));
    CHECK(response->errorCode == ix::HttpErrorCode::ChunkReadError);
    CHECK_FALSE(response->errorMsg.empty());
}

TEST_CASE("Streamed Content-Length responses retain their byte count", "[http][response]")
{
    ResponseServer server([](const ix::HttpRequestPtr&, size_t)
                          { return "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nabcde"; });
    server.startServing();
    ix::HttpClient client;
    auto args = options(client);
    std::string received;
    args->onChunkCallback = [&](const std::string& bytes) { received += bytes; };
    auto response = client.get(server.url(), args);
    REQUIRE(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(received == "abcde");
    CHECK(response->body.empty());
    CHECK(response->downloadSize == 5);
}
