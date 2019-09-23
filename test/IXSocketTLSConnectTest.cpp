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
        ix::WebSocketServer server(port,
                                   SocketServer::kDefaultHost,
                                   SocketServer::kDefaultTcpBacklog,
                                   SocketServer::kDefaultMaxConnections,
                                   60);
        server.setTLSOptions({".certs/trusted-server-crt.pem",
                              ".certs/trusted-server-key.pem", "NONE"
                              ".certs/trusted-ca-crt.pem"});
        REQUIRE(startWebSocketEchoServer(server));

        std::cerr << "server listening on port " << port << std::endl;
        // std::this_thread::sleep_for(std::chrono::minutes(60));

        ix::WebSocket client;
        auto url = "wss://127.0.0.1:" + std::to_string(port) + "/";
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

    // SECTION("Test connecting with an untrusted cert")
    // {
    //     std::string errMsg;
    //     std::string hostname("12313lhjlkjhopiupoijlkasdckljqwehrlkqjwehraospidcuaposidcasdc");
    //     int fd = SocketConnect::connect(hostname, 80, errMsg, [] { return false; });
    //     std::cerr << "Error message: " << errMsg << std::endl;
    //     REQUIRE(fd == -1);
    // }

    // SECTION("Test connecting with a trusted self-signed cert")
    // {
    //     int port = getFreePort();
    //     ix::WebSocketServer server(port);
    //     REQUIRE(startWebSocketEchoServer(server));

    //     std::string errMsg;
    //     // The callback returning true means we are requesting cancellation
    //     int fd = SocketConnect::connect("127.0.0.1", port, errMsg, [] { return true; });
    //     std::cerr << "Error message: " << errMsg << std::endl;
    //     REQUIRE(fd == -1);
    // }

    // SECTION("Test connecting with an untrusted self-signed cert")
    // {
    //     int port = getFreePort();
    //     ix::WebSocketServer server(port);
    //     REQUIRE(startWebSocketEchoServer(server));

    //     std::string errMsg;
    //     // The callback returning true means we are requesting cancellation
    //     int fd = SocketConnect::connect("127.0.0.1", port, errMsg, [] { return true; });
    //     std::cerr << "Error message: " << errMsg << std::endl;
    //     REQUIRE(fd == -1);
    // }
}
