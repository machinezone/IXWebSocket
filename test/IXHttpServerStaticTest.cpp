/*
 *  IXHttpServerStaticTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <catch_amalgamated.hpp>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXHttpServer.h>

namespace
{
    struct File
    {
        std::string name =
            "static-test-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin";
        explicit File(const std::string& bytes)
        {
            std::ofstream file(name, std::ios::binary);
            REQUIRE(file.is_open());
            file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            REQUIRE(file.good());
        }
        ~File()
        {
            std::remove(name.c_str());
        }
    };

    ix::HttpResponsePtr get(const std::string& path)
    {
        ix::HttpServer server(ix::getFreePort(), "127.0.0.1");
        server.setLogCallback([](ix::LogLevel, const std::string&) { });
        REQUIRE(server.listen().first);
        server.start();
        ix::HttpClient client;
        auto args = client.createRequest();
        args->compress = false;
        args->connectTimeout = 1;
        args->transferTimeout = 1;
        return client.get("http://127.0.0.1:" + std::to_string(server.getPort()) + "/" + path,
                          args);
    }
} // namespace

TEST_CASE("Static files preserve empty and binary content", "[http][static]")
{
    const std::string bytes = GENERATE(std::string(), std::string("A\r\nB\x1a\0Z", 7));
    File file(bytes);
    auto response = get(file.name);
    REQUIRE(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(response->statusCode == 200);
    CHECK(response->body == bytes);
}

TEST_CASE("Static files ignore URI query parameters", "[http][static]")
{
    File file("data");
    auto response = get(file.name + "?version=1/../other");
    REQUIRE(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(response->statusCode == 200);
    CHECK(response->body == "data");
}

TEST_CASE("Directories are not loaded as static files", "[http][static]")
{
    auto response = get("static-directory");
    REQUIRE(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(response->statusCode == 404);
}
