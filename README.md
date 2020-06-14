## Hello world

IXWebSocket is a C++ library for WebSocket client and server development. It has minimal dependencies (no boost), is very simple to use and support everything you'll likely need for websocket dev (SSL, deflate compression, compiles on most platforms, etc...). HTTP client and server code is also available, but it hasn't received as much testing.

It is been used on big mobile video game titles sending and receiving tons of messages since 2017 (iOS and Android). It was tested on macOS, iOS, Linux, Android, Windows and FreeBSD. Two important design goals are simplicity and correctness.

```cpp
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
```

Interested? Go read the [docs](https://machinezone.github.io/IXWebSocket/)! If things don't work as expected, please create an issue on GitHub, or even better a pull request if you know how to fix your problem.

IXWebSocket is actively being developed, check out the [changelog](https://machinezone.github.io/IXWebSocket/CHANGELOG/) to know what's cooking. If you are looking for a real time messaging service (the chat-like 'server' your websocket code will talk to) with many features such as history, backed by Redis, look at [cobra](https://github.com/machinezone/cobra).

IXWebSocket client code is autobahn compliant beginning with the 6.0.0 version. See the current [test results](https://bsergean.github.io/autobahn/reports/clients/index.html). Some tests are still failing in the server code.

## Users

If your company or project is using this library, feel free to open an issue or PR to amend this list.

- [Machine Zone](https://www.mz.com)
- [Tokio](https://gitlab.com/HCInk/tokio), a discord library focused on audio playback with node bindings.
- [libDiscordBot](https://github.com/tostc/libDiscordBot/tree/master), an easy to use Discord-bot framework.
- [gwebsocket](https://github.com/norrbotten/gwebsocket), a websocket (lua) module for Garry's Mod
- [DisCPP](https://github.com/DisCPP/DisCPP), a simple but feature rich Discord API wrapper

## Continuous Integration

| OS                | TLS               | Sanitizer         | Status            |
|-------------------|-------------------|-------------------|-------------------|
| Linux             | OpenSSL           | None              | [![Build2][1]][7] |
| macOS             | Secure Transport  | Thread Sanitizer  | [![Build2][2]][7] |
| macOS             | OpenSSL           | Thread Sanitizer  | [![Build2][3]][7] |
| macOS             | MbedTLS           | Thread Sanitizer  | [![Build2][4]][7] |
| Windows           | Disabled          | None              | [![Build2][5]][7] |
| UWP               | Disabled          | None              | [![Build2][6]][7] |

[1]: https://github.com/machinezone/IXWebSocket/workflows/linux/badge.svg
[2]: https://github.com/machinezone/IXWebSocket/workflows/mac_tsan_sectransport/badge.svg
[3]: https://github.com/machinezone/IXWebSocket/workflows/mac_tsan_openssl/badge.svg
[4]: https://github.com/machinezone/IXWebSocket/workflows/mac_tsan_mbedtls/badge.svg
[5]: https://github.com/machinezone/IXWebSocket/workflows/windows/badge.svg
[6]: https://github.com/machinezone/IXWebSocket/workflows/uwp/badge.svg
[7]: https://github.com/machinezone/IXWebSocket

