# General

## Introduction

[*WebSocket*](https://en.wikipedia.org/wiki/WebSocket) is a computer communications protocol, providing full-duplex
communication channels over a single TCP connection. This library provides a C++ library for Websocket communication. The code is derived from [easywsclient](https://github.com/dhbaird/easywsclient) and from the [Satori C SDK](https://github.com/satori-com/satori-rtm-sdk-c). It has been tested on the following platforms.

* macOS
* iOS
* Linux
* Android 
* Windows (no TLS support yet)

## Examples

The examples folder countains a simple chat program, using a node.js broadcast server.

Here is what the API looks like.

```
ix::WebSocket webSocket;

std::string url("ws://localhost:8080/");
webSocket.configure(url);

// Setup a callback to be fired when a message or an event (open, close, error) is received
webSocket.setOnMessageCallback(
    [](ix::WebSocketMessageType messageType, const std::string& str, ix::WebSocketErrorInfo error)
    {
        if (messageType == ix::WebSocket_MessageType_Message)
        {
            std::cout << str << std::endl;
        }
});

// Now that our callback is setup, we can start our background thread and receive messages
webSocket.start();

// Send a message to the server
webSocket.send("hello world");

// ... finally ...

// Stop the connection
webSocket.stop()
```

## Build

CMakefiles for the library and the examples are available. This library has few dependencies, so it is possible to just add the source files into your project.

## Implementation details

### TLS/SSL

Connections can be optionally secured and encrypted with TLS/SSL when using a wss:// endpoint, or using normal un-encrypted socket with ws:// endpoints. AppleSSL is used on iOS and macOS, and OpenSSL is used on Android and Linux.

### Polling and background thread work

No manual polling to fetch data is required. Data is sent and received instantly by using a background thread for receiving data and the select [system](http://man7.org/linux/man-pages/man2/select.2.html) call to be notified by the OS of incoming data. No timeout is used for select so that the background thread is only woken up when data is available, to optimize battery life. This is also the recommended way of using select according to the select tutorial, section [select law](https://linux.die.net/man/2/select_tut). Read and Writes to the socket are non blocking. Data is sent right away and not enqueued by writing directly to the socket, which is [possible](https://stackoverflow.com/questions/1981372/are-parallel-calls-to-send-recv-on-the-same-socket-valid) since system socket implementations allow concurrent read/writes. However concurrent writes need to be protected with mutex.

### Automatic reconnection

If the remote end (server) breaks the connection, the code will try to perpetually reconnect, by using an exponential backoff strategy, capped at one retry every 10 seconds.

## Limitations

* There is no per message compression support. That could be useful for retrieving large messages, but could also be implemented at the application level.
* There is no text support for sending data, only the binary protocol is supported. Sending json or text over the binary protocol works well.
* Automatic reconnection works at the TCP socket level, and will detect remote end disconnects. However, if the device/computer network become unreachable (by turning off wifi), it is quite hard to reliably and timely detect it at the socket level using `recv` and `send` error codes. [Here](https://stackoverflow.com/questions/14782143/linux-socket-how-to-detect-disconnected-network-in-a-client-program) is a good discussion on the subject. This behavior is consistent with other runtimes such as node.js. One way to detect a disconnected device with low level C code is to do a name resolution with DNS but this can be expensive. Mobile devices have good and reliable API to do that.

## Examples

1. Bring up a terminal and jump to the examples folder.
2. Compile the example C++ code. `sh build.sh`
3. Install node.js from [here](https://nodejs.org/en/download/).
4. Type `npm install` to install the node.js dependencies. Then `node broadcast-server.js` to run the server.
5. Bring up a second terminal. `./cmd_websocket_chat bob`
6. Bring up a third terminal. `./cmd_websocket_chat bill`
7. Start typing things in any of those terminals. Hopefully you should see your message being received on the other end.

## C++ code organization

Here's a simplistic diagram which explains how the code is structured in term of class/modules.

```
+-----------------------+
|                       | Start the receiving Background thread. Auto reconnection. Simple websocket Ping.
|  IXWebSocket          | Interface used by C++ test clients. No IX dependencies.
|                       | 
+-----------------------+
|                       |
|  IXWebSocketTransport | Low level websocket code, framing, managing raw socket. Adapted from easywsclient.
|                       |
+-----------------------+
|                       |
|  IXWebSocket          | ws://  Unencrypted Socket handler
|  IXWebSocketAppleSSL  | wss:// TLS encrypted Socket AppleSSL handler. Used on iOS and macOS
|  IXWebSocketOpenSSL   | wss:// TLS encrypted Socket OpenSSL handler.  Used on Android and Linux
|                       |                                               Can be used on macOS too.
+-----------------------+
```

## API

### Sending messages

`websocket.send("foo")` will send a message.

If the connection was closed and sending failed, the return value will be set to false.

### ReadyState

`getReadyState()` returns the state of the connection. There are 4 possible states.

1. WebSocket_ReadyState_Connecting - The connection is not yet open.
2. WebSocket_ReadyState_Open       - The connection is open and ready to communicate.
3. WebSocket_ReadyState_Closing    - The connection is in the process of closing.
4. WebSocket_MessageType_Close     - The connection is closed or couldn't be opened.

### Open and Close notifications

The onMessage event will be fired when the connection is opened or closed. This is similar to the [Javascript browser API](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket), which has `open` and `close` events notification that can be registered with the browser `addEventListener`.

```
webSocket.setOnMessageCallback(
    [this](ix::WebSocketMessageType messageType, const std::string& str, ix::WebSocketErrorInfo error)
    {
        if (messageType == ix::WebSocket_MessageType_Open)
        {
            puts("send greetings");
        }
        else if (messageType == ix::WebSocket_MessageType_Close)
        {
            puts("disconnected");
        }
    }
);
```

### Error notification

A message will be fired when there is an error with the connection. The message type will be `ix::WebSocket_MessageType_Error`. Multiple fields will be available on the event to describe the error.

```
webSocket.setOnMessageCallback(
    [this](ix::WebSocketMessageType messageType, const std::string& str, ix::WebSocketErrorInfo error)
    {
        if (messageType == ix::WebSocket_MessageType_Error)
        {
            std::stringstream ss;
            ss << "Error: "         << error.reason      << std::endl;
            ss << "#retries: "      << event.retries     << std::endl;
            ss << "Wait time(ms): " << event.wait_time   << std::endl;
            ss << "HTTP Status: "   << event.http_status << std::endl;
            std::cout << ss.str() << std::endl;
        }
    }
);
```

### start, stop

1. `websocket.start()` connect to the remote server and starts the message receiving background thread.
2. `websocket.stop()` disconnect from the remote server and closes the background thread.

### Configuring the remote url

The url can be set and queried after a websocket object has been created. You will have to call `stop` and `start` if you want to disconnect and connect to that new url.

```
std::string url("wss://example.com");
websocket.configure(url);
```

### Ping/Pong support

Ping/pong messages are used to implement keep-alive. 2 message types exists to identify ping and pong messages. Note that when a ping message is received, a pong is instantly send back as requested by the WebSocket spec.

```
webSocket.setOnMessageCallback(
    [this](ix::WebSocketMessageType messageType, const std::string& str, ix::WebSocketErrorInfo error)
    {
        if (messageType == ix::WebSocket_MessageType_Ping ||
            messageType == ix::WebSocket_MessageType_Pong)
        {
            std::cout << "pong data: " << str << std::endl;
        }
    }
);
```

A ping message can be sent to the server, with an optional data string.

```
websocket.ping("ping data, optional (empty string is ok): limited to 125 bytes long");
```
