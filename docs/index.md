![Alt text](https://travis-ci.org/machinezone/IXWebSocket.svg?branch=master)

## Introduction

[*WebSocket*](https://en.wikipedia.org/wiki/WebSocket) is a computer communications protocol, providing full-duplex and bi-directionnal communication channels over a single TCP connection. *IXWebSocket* is a C++ library for client and server Websocket communication, and for client and server HTTP communication. *TLS* aka *SSL* is supported. The code is derived from [easywsclient](https://github.com/dhbaird/easywsclient) and from the [Satori C SDK](https://github.com/satori-com/satori-rtm-sdk-c). It has been tested on the following platforms.

* macOS
* iOS
* Linux
* Android
* Windows
* FreeBSD

## Example code

```cpp
// Required on Windows
ix::initNetSystem();

// Our websocket object
ix::WebSocket webSocket;

std::string url("ws://localhost:8080/");
webSocket.setUrl(url);

// Setup a callback to be fired when a message or an event (open, close, error) is received
webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr& msg)
    {
        if (msg->type == ix::WebSocketMessageType::Message)
        {
            std::cout << msg->str << std::endl;
        }
    }
);

// Now that our callback is setup, we can start our background thread and receive messages
webSocket.start();

// Send a message to the server (default to TEXT mode)
webSocket.send("hello world");
```

## Why another library?

There are 2 main reasons that explain why IXWebSocket got written. First, we needed a C++ cross-platform client library, which should have few dependencies. What looked like the most solid one, [websocketpp](https://github.com/zaphoyd/websocketpp) did depend on boost and this was not an option for us. Secondly, there were other available libraries with fewer dependencies (C ones), but they required calling an explicit poll routine periodically to know if a client had received data from a server, which was not elegant.

We started by solving those 2 problems, then we added server websocket code, then an HTTP client, and finally a very simple HTTP server.

## Contributing

IXWebSocket is developed on [GitHub](https://github.com/machinezone/IXWebSocket). We'd love to hear about how you use it; opening up an issue on GitHub is ok for that. If things don't work as expected, please create an issue on GitHub, or even better a pull request if you know how to fix your problem.
