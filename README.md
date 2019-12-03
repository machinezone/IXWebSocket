## Hello world

![Build status badge](https://travis-ci.org/machinezone/IXWebSocket.svg?branch=master)

IXWebSocket is a C++ library for WebSocket client and server development. It has minimal dependencies (no boost), is very simple to use and support everything you'll likely need for websocket dev (SSL, deflate compression, compiles on most platforms, etc...). HTTP client and server code is also available, but it hasn't received as much testing.

It is been used on big mobile video game titles sending and receiving tons of messages since 2017 (iOS and Android). It was tested on macOS, iOS, Linux, Android, Windows and FreeBSD. Two important design goals are simplicity and correctness.

```cpp
// Required on Windows
ix::initNetSystem();

// Our websocket object
ix::WebSocket webSocket;

std::string url("ws://localhost:8080/");
webSocket.setUrl(url);

// Setup a callback to be fired (in a background thread, watch out for race conditions !)
// when a message or an event (open, close, error) is received
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

Interested? Go read the [docs](https://machinezone.github.io/IXWebSocket/)! If things don't work as expected, please create an issue on GitHub, or even better a pull request if you know how to fix your problem.

IXWebSocket is actively being developed, check out the [changelog](https://machinezone.github.io/IXWebSocket/CHANGELOG/) to know what's cooking. If you are looking for a real time messaging service (the chat-like 'server' your websocket code will talk to) with many features such as history, backed by Redis, look at [cobra](https://github.com/machinezone/cobra).

IXWebSocket client code is autobahn compliant beginning with the 6.0.0 version. See the current [test results](https://bsergean.github.io/IXWebSocket/autobahn/index.html). Some tests are still failing in the server code.
