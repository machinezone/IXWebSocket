/*
 *  IXHttpClientAsyncTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <catch_amalgamated.hpp>
#include <chrono>
#include <cstring>
#include <future>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXHttpServer.h>

namespace
{
    class ReleaseGate
    {
    public:
        ReleaseGate()
            : ready(_release.get_future().share())
        {
        }

        ~ReleaseGate()
        {
            open();
        }

        void open()
        {
            if (!_opened)
            {
                _opened = true;
                _release.set_value();
            }
        }

    private:
        std::promise<void> _release;
        bool _opened = false;

    public:
        const std::shared_future<void> ready;
    };

    bool ready(std::future<ix::HttpResponsePtr>& response)
    {
        return response.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    }

    std::string url(ix::HttpServer& server)
    {
        return "http://127.0.0.1:" + std::to_string(server.getPort());
    }
} // namespace

TEST_CASE("Queued HTTP requests retain their submitted configuration", "[http][async]")
{
    const bool streaming = GENERATE(false, true);
    ix::HttpServer server(ix::getFreePort(), "127.0.0.1");
    server.setOnConnectionCallback(
        [](ix::HttpRequestPtr request, std::shared_ptr<ix::ConnectionState>)
        {
            return std::make_shared<ix::HttpResponse>(200,
                                                      "OK",
                                                      ix::HttpErrorCode::Ok,
                                                      ix::WebSocketHttpHeaders(),
                                                      request->method + " " + request->uri + " " +
                                                          request->body + " " +
                                                          request->headers["X-Request"]);
        });
    REQUIRE(server.listen().first);
    server.start();

    std::promise<void> entered;
    std::promise<ix::HttpResponsePtr> first;
    std::promise<ix::HttpResponsePtr> second;
    auto firstResult = first.get_future();
    auto secondResult = second.get_future();
    ix::HttpClient client(true);
    // Declared after client: release its callback before client joins on any test failure.
    ReleaseGate gate;
    auto resume = gate.ready;
    REQUIRE(client.performRequest(client.createRequest(url(server), ix::HttpClient::kGet),
                                  [&entered, resume](const ix::HttpResponsePtr&)
                                  {
                                      entered.set_value();
                                      resume.wait();
                                  }));
    REQUIRE(entered.get_future().wait_for(std::chrono::seconds(2)) == std::future_status::ready);

    auto args = client.createRequest(url(server) + "/first", ix::HttpClient::kPost);
    const auto setBody = [&](const std::string& body)
    {
        if (streaming)
        {
            args->bodySize = body.size();
            args->onUploadCallback = [body](uint64_t offset, char* buffer, size_t capacity)
            {
                const auto count = (std::min) (capacity, body.size() - static_cast<size_t>(offset));
                std::memcpy(buffer, body.data() + static_cast<size_t>(offset), count);
                return count;
            };
        }
        else
        {
            args->body = body;
        }
    };
    setBody("first-body");
    args->extraHeaders["X-Request"] = "first-header";
    REQUIRE(client.performRequest(
        args, [&first](const ix::HttpResponsePtr& response) { first.set_value(response); }));
    args->url = url(server) + "/second";
    args->verb = ix::HttpClient::kPut;
    setBody("second-body");
    args->extraHeaders["X-Request"] = "second-header";
    REQUIRE(client.performRequest(
        args, [&second](const ix::HttpResponsePtr& response) { second.set_value(response); }));
    gate.open();

    REQUIRE(ready(firstResult));
    REQUIRE(ready(secondResult));
    const auto a = firstResult.get();
    const auto b = secondResult.get();
    CHECK(a->errorCode == ix::HttpErrorCode::Ok);
    CHECK(b->errorCode == ix::HttpErrorCode::Ok);
    CHECK(a->body == "POST /first first-body first-header");
    CHECK(b->body == "PUT /second second-body second-header");
}

TEST_CASE("Queued HTTP requests retain their cancellation handle", "[http][async]")
{
    ix::HttpServer server(ix::getFreePort(), "127.0.0.1");
    server.setOnConnectionCallback([](ix::HttpRequestPtr, std::shared_ptr<ix::ConnectionState>)
                                   { return std::make_shared<ix::HttpResponse>(200, "OK"); });
    REQUIRE(server.listen().first);
    server.start();
    std::promise<void> entered;
    std::promise<ix::HttpResponsePtr> completed;
    auto result = completed.get_future();
    ix::HttpClient client(true);
    ReleaseGate gate;
    auto resume = gate.ready;
    REQUIRE(client.performRequest(client.createRequest(url(server), ix::HttpClient::kGet),
                                  [&entered, resume](const ix::HttpResponsePtr&)
                                  {
                                      entered.set_value();
                                      resume.wait();
                                  }));
    REQUIRE(entered.get_future().wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    auto args = client.createRequest(url(server), ix::HttpClient::kGet);
    REQUIRE(client.performRequest(args,
                                  [&completed](const ix::HttpResponsePtr& response)
                                  { completed.set_value(response); }));
    args->cancel = true;
    gate.open();
    REQUIRE(ready(result));
    CHECK(result.get()->errorCode == ix::HttpErrorCode::Cancelled);
}

TEST_CASE("Active HTTP cancellation survives redirects", "[http][async]")
{
    const bool redirect = GENERATE(false, true);
    std::promise<void> arrived;
    auto requestArrived = arrived.get_future();
    // This gate outlives server workers; the separate release guard below opens it before shutdown.
    ReleaseGate responseGate;
    ix::HttpServer server(ix::getFreePort(), "127.0.0.1");
    auto resume = responseGate.ready;
    server.setLogCallback([](ix::LogLevel, const std::string&) { });
    server.setOnConnectionCallback(
        [&server, &arrived, resume](ix::HttpRequestPtr request,
                                    std::shared_ptr<ix::ConnectionState>)
        {
            if (request->uri == "/redirect")
            {
                return std::make_shared<ix::HttpResponse>(
                    302,
                    "Found",
                    ix::HttpErrorCode::Ok,
                    ix::WebSocketHttpHeaders {{"Location", url(server) + "/slow"}});
            }
            arrived.set_value();
            resume.wait();
            return std::make_shared<ix::HttpResponse>(200, "OK");
        });
    REQUIRE(server.listen().first);
    server.start();
    std::promise<ix::HttpResponsePtr> completed;
    auto result = completed.get_future();
    ix::HttpClient client(true);
    struct OpenOnExit
    {
        ReleaseGate& gate;
        ~OpenOnExit()
        {
            gate.open();
        }
    } release {responseGate};
    auto args = client.createRequest(url(server) + (redirect ? "/redirect" : "/slow"),
                                     ix::HttpClient::kGet);
    REQUIRE(client.performRequest(args,
                                  [&completed](const ix::HttpResponsePtr& response)
                                  { completed.set_value(response); }));
    REQUIRE(requestArrived.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    args->cancel = true;
    REQUIRE(ready(result));
    CHECK(result.get()->errorCode == ix::HttpErrorCode::Cancelled);
}
