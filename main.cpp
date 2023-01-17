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
 *
 *  Or use cmake -DBUILD_DEMO=ON option for other platform
 */

#include <iostream>
#include <ixwebsocket/IXUserAgent.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/ProxySoctekSetting.h>

int main()
{
    // Required on Windows
    //ix::initNetSystem();

    // Our websocket object
    ix::WebSocket webSocket;
    ix::SocketTLSOptions tr;
    tr.tls = true;
    tr.caFile = "NONE";
   webSocket.setTLSOptions(tr);
    std::string  url("wss://ws.postman-echo.com/raw");

    ProxySetup setup_params;
    ProxyConnectionType proxy_con_type_choice = ProxyConnectionType::connection_web;

    std::string proxytype("WEB");
    std::string proxyhost("proxy.emea.etn.com");
    setup_params.setProxyHost(proxyhost);
    setup_params.setProxyPort(8080);
    setup_params.setProxyType(proxy_con_type_choice);

    webSocket.setProxySettings(std::ref(setup_params));
    webSocket.setUrl(url);


    std::cout << ix::userAgent() << std::endl;
    std::cout << "Connecting to " << url << "..." << std::endl;

    // Setup a callback to be fired (in a background thread, watch out for race conditions !)
    // when a message or an event (open, close, error) is received
    webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr& msg)
        {
            if (msg->type == ix::WebSocketMessageType::Message)
            {
                std::cout << "received message: " << msg->str << std::endl;
                std::cout << "> " << std::flush;
            }
            else if (msg->type == ix::WebSocketMessageType::Open)
            {
                std::cout << "Connection established" << std::endl;
                std::cout << "> " << std::flush;
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                // Maybe SSL is not configured properly
                std::cout << "Connection error: " << msg->errorInfo.reason << std::endl;
                std::cout << "> " << std::flush;
            }
        }
    );

    // Now that our callback is setup, we can start our background thread and receive messages
    webSocket.start();

    // Send a message to the server (default to TEXT mode)
    webSocket.send("hello world");

    // Display a prompt
    std::cout << "> " << std::flush;

    std::string text;
    // Read text from the console and send messages in text mode.
    // Exit with Ctrl-D on Unix or Ctrl-Z on Windows.
    while (std::getline(std::cin, text))
    {
        webSocket.send(text);
        std::cout << "> " << std::flush;
    }

    return 0;
}
