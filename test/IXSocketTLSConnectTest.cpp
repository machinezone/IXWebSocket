/*
 *  IXSocketConnectTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include "catch.hpp"
#include <chrono>
#include <iostream>
#include <ixwebsocket/IXSocketConnect.h>
#include <ixwebsocket/IXWebSocket.h>

using namespace ix;


TEST_CASE("tls_socket_connect", "[net]")
{
    SECTION("Test connecting with a trusted cert")
    {
        int port = getFreePort();
        ix::WebSocketServer server(port);
        server.setTLSOptions({".certs/trusted-server-crt.pem",
                              ".certs/trusted-server-key.pem",
                              ".certs/trusted-ca-crt.pem"});
        REQUIRE(startWebSocketEchoServer(server));

        std::cerr << "server listening on port " << port << std::endl;

        ix::WebSocket client;
        auto url = "wss://localhost:" + std::to_string(port) + "/";
        client.setUrl(url);
        client.setTLSOptions({".certs/trusted-client-crt.pem",
                              ".certs/trusted-client-key.pem",
                              ".certs/trusted-ca-crt.pem"});
        client.setOnMessageCallback([](const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Message)
            {
                std::cerr << "Client received: " << msg->str << std::endl;
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                std::stringstream ss;
                ss << "Error: " << msg->errorInfo.reason << std::endl;
                ss << "#retries: " << msg->errorInfo.retries << std::endl;
                ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                std::cerr << ss.str() << std::endl;
            }
        });
        client.start();
        auto timed_out = std::chrono::system_clock::now() + std::chrono::seconds(2);
        while (client.getReadyState() != ix::ReadyState::Open &&
               std::chrono::system_clock::now() < timed_out)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        auto status = client.sendText("Hello");
        client.stop();
        REQUIRE(status.success);
    }

    SECTION("Test connecting with an untrusted cert")
    {
        int port = getFreePort();
        ix::WebSocketServer server(port);
        server.setTLSOptions({".certs/trusted-server-crt.pem",
                              ".certs/trusted-server-key.pem",
                              ".certs/trusted-ca-crt.pem"});
        REQUIRE(startWebSocketEchoServer(server));

        std::cerr << "server listening on port " << port << std::endl;

        ix::WebSocket client;
        auto url = "wss://localhost:" + std::to_string(port) + "/";

        ix::WebSocketErrorInfo errInfo;
        client.setUrl(url);
        client.setTLSOptions({".certs/untrusted-client-crt.pem",
                              ".certs/untrusted-client-key.pem",
                              ".certs/trusted-ca-crt.pem"});
        client.setOnMessageCallback([&errInfo](const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Message)
            {
                std::cerr << "Client received: " << msg->str << std::endl;
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                std::stringstream ss;
                ss << "Error: " << msg->errorInfo.reason << std::endl;
                ss << "#retries: " << msg->errorInfo.retries << std::endl;
                ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                std::cerr << ss.str() << std::endl;
                errInfo = msg->errorInfo;
            }
        });
        client.start();
        auto timed_out = std::chrono::system_clock::now() + std::chrono::seconds(2);
        while (client.getReadyState() != ix::ReadyState::Open &&
               std::chrono::system_clock::now() < timed_out)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        auto status = client.sendText("Hello");
        client.stop();
        REQUIRE(status.success == false);

        using Catch::Matchers::Contains;
        REQUIRE_THAT(errInfo.reason, Contains("tlsv1 alert unknown ca"));
    }

    SECTION("Test connecting with a trusted self-signed cert")
    {
        int port = getFreePort();
        ix::WebSocketServer server(port);
        server.setTLSOptions({".certs/trusted-server-crt.pem",
                              ".certs/trusted-server-key.pem",
                              ".certs/selfsigned-client-crt.pem"});
        REQUIRE(startWebSocketEchoServer(server));

        std::cerr << "server listening on port " << port << std::endl;

        ix::WebSocket client;
        auto url = "wss://localhost:" + std::to_string(port) + "/";

        ix::WebSocketErrorInfo errInfo;
        client.setUrl(url);
        client.setTLSOptions({".certs/selfsigned-client-crt.pem",
                              ".certs/selfsigned-client-key.pem",
                              ".certs/trusted-ca-crt.pem"});
        client.setOnMessageCallback([&errInfo](const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Message)
            {
                std::cerr << "Client received: " << msg->str << std::endl;
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                std::stringstream ss;
                ss << "Error: " << msg->errorInfo.reason << std::endl;
                ss << "#retries: " << msg->errorInfo.retries << std::endl;
                ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                std::cerr << ss.str() << std::endl;
                errInfo = msg->errorInfo;
            }
        });
        client.start();
        auto timed_out = std::chrono::system_clock::now() + std::chrono::seconds(2);
        while (client.getReadyState() != ix::ReadyState::Open &&
               std::chrono::system_clock::now() < timed_out)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        auto status = client.sendText("Hello");
        client.stop();
        REQUIRE(status.success);
    }

    SECTION("Test connecting with a trusted self-signed cert, no client verification")
    {
        int port = getFreePort();
        ix::WebSocketServer server(port);
        server.setTLSOptions({".certs/trusted-server-crt.pem",
                              ".certs/trusted-server-key.pem",
                              ".certs/selfsigned-client-crt.pem"});
        REQUIRE(startWebSocketEchoServer(server));

        std::cerr << "server listening on port " << port << std::endl;

        ix::WebSocket client;
        auto url = "wss://localhost:" + std::to_string(port) + "/";

        ix::WebSocketErrorInfo errInfo;
        client.setUrl(url);
        client.setTLSOptions(
            {".certs/selfsigned-client-crt.pem", ".certs/selfsigned-client-key.pem", "NONE"});
        client.setOnMessageCallback([&errInfo](const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Message)
            {
                std::cerr << "Client received: " << msg->str << std::endl;
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                std::stringstream ss;
                ss << "Error: " << msg->errorInfo.reason << std::endl;
                ss << "#retries: " << msg->errorInfo.retries << std::endl;
                ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                std::cerr << ss.str() << std::endl;
                errInfo = msg->errorInfo;
            }
        });
        client.start();
        auto timed_out = std::chrono::system_clock::now() + std::chrono::seconds(2);
        while (client.getReadyState() != ix::ReadyState::Open &&
               std::chrono::system_clock::now() < timed_out)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        auto status = client.sendText("Hello");
        client.stop();
        REQUIRE(status.success);
    }

    SECTION("client uses trusted self-signed cert, untrusted server, no client verification, ALL "
            "ciphers")
    {
        int port = getFreePort();
        ix::WebSocketServer server(port);
        server.setTLSOptions({".certs/trusted-server-crt.pem",
                              ".certs/trusted-server-key.pem",
                              ".certs/selfsigned-client-crt.pem"});
        REQUIRE(startWebSocketEchoServer(server));

        std::cerr << "server listening on port " << port << std::endl;

        ix::WebSocket client;
        auto url = "wss://localhost:" + std::to_string(port) + "/";

        ix::WebSocketErrorInfo errInfo;
        client.setUrl(url);
        client.setTLSOptions({
            ".certs/selfsigned-client-crt.pem",
            ".certs/selfsigned-client-key.pem",
            "NONE", // client doesn't verify server
            "ALL"   // client is okay with any possible cipher
        });
        client.setOnMessageCallback([&errInfo](const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Message)
            {
                std::cerr << "Client received: " << msg->str << std::endl;
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                std::stringstream ss;
                ss << "Error: " << msg->errorInfo.reason << std::endl;
                ss << "#retries: " << msg->errorInfo.retries << std::endl;
                ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                std::cerr << ss.str() << std::endl;
                errInfo = msg->errorInfo;
            }
        });
        client.start();
        auto timed_out = std::chrono::system_clock::now() + std::chrono::seconds(2);
        while (client.getReadyState() != ix::ReadyState::Open &&
               std::chrono::system_clock::now() < timed_out)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        auto status = client.sendText("Hello");
        client.stop();
        REQUIRE(status.success);
    }
}
