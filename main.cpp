/*
 *  main.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 *
 *  Super simple standalone example. See ws folder, unittest and doc/usage.md for more.
 *
 *  On macOS
 *  $ mkdir -p build ; cd build ; cmake -DUSE_TLS=1 .. ; make -j ; make install
 *  $ clang++ --std=c++14 --stdlib=libc++ main.cpp -lixwebsocket -lz -framework Security -framework Foundation
 *  $ ./a.out
 */

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <iostream>

int main()
{
    // Required on Windows
    ix::initNetSystem();

    // Our websocket object
    ix::WebSocket webSocket;

    std::string url("wss://echo.websocket.org");
    webSocket.setUrl(url);

    std::cout << "Connecting to " << url << "..." << std::endl;

    // Setup a callback to be fired (in a background thread, watch out for race conditions !)
    // when a message or an event (open, close, error) is received
    webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr& msg)
        {
            if (msg->type == ix::WebSocketMessageType::Message)
            {
                std::cout << "received message: " << msg->str << std::endl;
            }
            else if (msg->type == ix::WebSocketMessageType::Open)
            {
                std::cout << "Connection established" << std::endl;
            }
        }
    );

    // Now that our callback is setup, we can start our background thread and receive messages
    webSocket.start();

    // Send a message to the server (default to TEXT mode)
    webSocket.send("hello world");

    while (true)
    {
        std::string text;
        std::cout << "> " << std::flush;
        std::getline(std::cin, text);

        webSocket.send(text);
    }

    return 0;
}
