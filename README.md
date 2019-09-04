## Hello world

![Alt text](https://travis-ci.org/machinezone/IXWebSocket.svg?branch=master) 

IXWebSocket is a C++ library for WebSocket client and server development. It has minimal dependencies (no boost), is very simple to use and support everything you'll likely need for websocket dev (SSL, deflate compression, compiles on most platforms, etc...). HTTP client and server code is also available, but it hasn't received as much testing.

It is been used on big mobile video game titles sending and receiving tons of messages since 2017 (iOS and Android).

Interested ? Go read the [docs](https://bsergean.github.io/IXWebSocket/site/) ! If things don't work as expected, please create an issue in github, or even better a pull request if you know how to fix your problem.

IXWebSocket is actively being developed, check out the [changelog](CHANGELOG.md) to know what's cooking. If you are looking for a real time messaging service (the chat-like 'server' your websocket code will talk to) with many features such as history, backed by Redis, look at [cobra](https://github.com/machinezone/cobra).

IXWebSocket is not yet autobahn compliant, but we are working on changing this. See the current compliance [test results](https://bsergean.github.io/IXWebSocket/autobahn/index.html). The only tests that are still failing are the Websocket Compression ones (see section 12 and 13).
