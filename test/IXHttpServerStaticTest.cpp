/*
 *  IXHttpServerStaticTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <algorithm>
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

    ix::HttpResponsePtr get(const std::string& path, const std::string& documentRoot = ".")
    {
        ix::HttpServer server(ix::getFreePort(), "127.0.0.1");
        server.setDocumentRoot(documentRoot);
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

TEST_CASE("Static files use the configured document root", "[http][static][document_root]")
{
    std::string root = GENERATE(std::string("document-root"),
                                std::string("document-root/"),
                                std::string(IX_TEST_DOCUMENT_ROOT),
                                std::string(IX_TEST_DOCUMENT_ROOT) + "/");
#ifdef _WIN32
    if (GENERATE(false, true)) std::replace(root.begin(), root.end(), '/', '\\');
#endif
    auto index = get("", root);
    REQUIRE(index->errorCode == ix::HttpErrorCode::Ok);
    CHECK(index->statusCode == 200);
    CHECK(index->body == "document root index");
    auto asset = get("asset.txt?version=1", root);
    REQUIRE(asset->errorCode == ix::HttpErrorCode::Ok);
    CHECK(asset->statusCode == 200);
    CHECK(asset->body == "document root asset");
}

TEST_CASE("An empty document root selects the current directory", "[http][static][document_root]")
{
    File file("current directory");
    auto response = get(file.name, "");
    REQUIRE(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(response->statusCode == 200);
    CHECK(response->body == "current directory");
}

TEST_CASE("Static paths stay within the configured document root", "[http][static][document_root]")
{
    File outside("outside document root");
    const std::string prefix =
        GENERATE(std::string(), std::string("../"), std::string("sub/../../"));
    auto response = get(prefix + outside.name, "document-root");
    REQUIRE(response->errorCode == ix::HttpErrorCode::Ok);
    CHECK(response->statusCode == 404);
#ifdef _WIN32
    auto windowsResponse = get("..\\" + outside.name, "document-root\\");
    REQUIRE(windowsResponse->errorCode == ix::HttpErrorCode::Ok);
    CHECK(windowsResponse->statusCode == 404);
#endif
}
