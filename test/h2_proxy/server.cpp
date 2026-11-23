/*
 * Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <iostream>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <mutex>
#include <string>

int main()
{
    ix::initNetSystem();
    {
        std::mutex logMutex;
        ix::WebSocketServer server(0, "127.0.0.1");
        server.setOnClientMessageCallback(
            [&](std::shared_ptr<ix::ConnectionState>,
                ix::WebSocket& socket,
                const ix::WebSocketMessagePtr& message)
            {
                if (message->type == ix::WebSocketMessageType::Open)
                {
                    const auto& headers = message->openInfo.headers;
                    const auto marker = headers.find("X-Proxy-Test");
                    const auto origin = headers.find("Origin");
                    if (marker == headers.end() || marker->second != "h2-integration" ||
                        origin == headers.end() || origin->second != "https://client.example")
                    {
                        socket.close(4003, "Proxy headers missing");
                    }
                    std::lock_guard<std::mutex> lock(logMutex);
                    std::cout << "OPEN " << message->openInfo.uri << std::endl;
                }
                else if (message->type == ix::WebSocketMessageType::Message)
                {
                    if (!message->binary && message->str == "close-from-server")
                    {
                        socket.close(4000, "server close test");
                    }
                    else
                    {
                        socket.send(message->str, message->binary);
                    }
                }
                else if (message->type == ix::WebSocketMessageType::Close)
                {
                    std::lock_guard<std::mutex> lock(logMutex);
                    std::cout << "CLOSE " << message->closeInfo.code << std::endl;
                }
            });
        const auto listening = server.listen();
        if (!listening.first)
        {
            std::cerr << listening.second << std::endl;
            return 1;
        }
        server.start();
        std::cout << "READY " << server.getPort() << std::endl;
        std::string command;
        std::getline(std::cin, command);
        server.stop();
    }
    ix::uninitNetSystem();
}
