# Examples

The [*ws*](https://github.com/machinezone/IXWebSocket/tree/master/ws) folder countains many interactive programs for chat, [file transfers](https://github.com/machinezone/IXWebSocket/blob/master/ws/ws_send.cpp), [curl like](https://github.com/machinezone/IXWebSocket/blob/master/ws/ws_http_client.cpp) http clients, demonstrating client and server usage.

## Windows note

To use the network system on Windows, you need to initialize it once with *WSAStartup()* and clean it up with *WSACleanup()*. We have helpers for that which you can use, see below. This init would typically take place in your main function. On every platform, *ix::uninitNetSystem()* also releases process-global TLS state (mbedTLS PSA crypto, with mbedTLS >= 3.6), so call it only after all sockets are closed.

```cpp
#include <ixwebsocket/IXNetSystem.h>

int main()
{
    ix::initNetSystem();

    ...

    ix::uninitNetSystem();
    return 0;
}
```

## WebSocket client API

```cpp
#include <ixwebsocket/IXWebSocket.h>

...

// Our websocket object
ix::WebSocket webSocket;

std::string url("ws://localhost:8080/");
webSocket.setUrl(url);

// Optional heart beat, sent every 45 seconds when there is not any traffic
// to make sure that load balancers do not kill an idle connection.
webSocket.setPingInterval(45);

// Per message deflate connection is not enabled by default. You can tweak its parameters, enable or disable it with
webSocket.enablePerMessageDeflate();
webSocket.disablePerMessageDeflate();

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

// The message can be sent in BINARY mode (useful if you send MsgPack data for example)
webSocket.sendBinary("some serialized binary data");

// ... finally ...

// Stop the connection
webSocket.stop()
```

### Sending messages

`WebSocketSendInfo result = websocket.send("foo")` will send a message.

If the connection was closed, sending will fail, and the success field of the result object  will be set to false. There could also be a compression error in which case the compressError field will be set to true. The payloadSize field and wireSize fields will tell you respectively how much bytes the message weight, and how many bytes were sent on the wire (potentially compressed + counting the message header (a few bytes).

There is an optional progress callback that can be passed in as the second argument. If a message is large it will be fragmented into chunks which will be sent independantly. Everytime the we can write a fragment into the OS network cache, the callback will be invoked. If a user wants to cancel a slow send, false should be returned from within the callback.

Here is an example code snippet copied from the ws send sub-command. Each fragment weights 32K, so the total integer is the wireSize divided by 32K. As an example if you are sending 32M of data, uncompressed, total will be 1000. current will be set to 0 for the first fragment, then 1, 2 etc...

```
auto result =
    _webSocket.sendBinary(serializedMsg, [this, throttle](int current, int total) -> bool {
        ix::logInfo("ws_send: Step {} out of {}", current + 1, total);

        if (throttle)
        {
            std::chrono::duration<double, std::milli> duration(10);
            std::this_thread::sleep_for(duration);
        }

        return _connected;
    });
```

The `send()` and `sendText()` methods check that the string contains only valid UTF-8 characters. If you know that the string is a valid UTF-8 string you can skip that step and use the `sendUtf8Text` method instead.

With the IXWebSocketSendData overloads of `sendUtf8Text` and `sendBinary` it is possible to not only send std::string but also `std::vector<char>`, `std::vector<uint8_t>` and `char*`.

```
std::vector<uint8_t> data({1, 2, 3, 4});
auto result = webSocket.sendBinary(data);

const char* text = "Hello World!";
result = webSocket.sendUtf8Text(IXWebSocketSendData(text, strlen(text)));
```

### Incoming message size limit

Incoming messages are unlimited by default. Set a limit in bytes before connecting
or starting the server:

```cpp
webSocket.setMaxMessageSize(1024 * 1024); // Client: 1 MiB per incoming message.
server.setMaxMessageSize(1024 * 1024);    // Server: default for new connections.
```

`getMaxMessageSize()` returns the configured limit; `setMaxMessageSize(0)` restores
unlimited messages. Client settings apply on the next connection, including
reconnections. Server settings also apply to WebSocket upgrades through `HttpServer`.

The limit covers the combined payload of all fragments, both before and after
per-message decompression. Control frames do not count toward the limit. Oversized
messages close the connection with code **1009** (Message Too Big), without
delivering that message. Frame lengths are checked before buffering their payload;
decompression stops before appending output beyond the limit.

This is a per-message limit, rather than a total process memory budget. Choose it
alongside your connection limit and application buffering requirements.

The echo server exposes the same setting:

```sh
ws echo_server --port 8080 --max-message-size 1048576
```

### ReadyState

`getReadyState()` returns the state of the connection. There are 4 possible states.

1. ReadyState::Connecting - The connection is not yet open.
2. ReadyState::Open       - The connection is open and ready to communicate.
3. ReadyState::Closing    - The connection is in the process of closing.
4. ReadyState::Closed     - The connection is closed or could not be opened.

### Open and Close notifications

The onMessage event will be fired when the connection is opened or closed. This is similar to the [JavaScript browser API](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket), which has `open` and `close` events notification that can be registered with the browser `addEventListener`.

```cpp
webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr& msg)
    {
        if (msg->type == ix::WebSocketMessageType::Open)
        {
            std::cout << "send greetings" << std::endl;

            // Headers can be inspected (pairs of string/string)
            std::cout << "Handshake Headers:" << std::endl;
            for (auto it : msg->headers)
            {
                std::cout << it.first << ": " << it.second << std::endl;
            }
        }
        else if (msg->type == ix::WebSocketMessageType::Close)
        {
            std::cout << "disconnected" << std::endl;

            // The server can send an explicit code and reason for closing.
            // This data can be accessed through the closeInfo object.
            std::cout << msg->closeInfo.code << std::endl;
            std::cout << msg->closeInfo.reason << std::endl;
        }
    }
);
```

### Error notification

A message will be fired when there is an error with the connection. The message type will be `ix::WebSocketMessageType::Error`. Multiple fields will be available on the event to describe the error.

When a server rejects the WebSocket handshake, `msg->errorInfo.headers` contains the HTTP response headers, such as `Retry-After` or custom authentication headers. Header names are case insensitive. Errors without response headers leave this map empty.

```cpp
webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr& msg)
    {
        if (msg->type == ix::WebSocketMessageType::Error)
        {
            std::stringstream ss;
            ss << "Error: "         << msg->errorInfo.reason      << std::endl;
            ss << "#retries: "      << msg->errorInfo.retries     << std::endl;
            ss << "Wait time(ms): " << msg->errorInfo.wait_time   << std::endl;
            ss << "HTTP Status: "   << msg->errorInfo.http_status << std::endl;
            std::cout << ss.str() << std::endl;
        }
    }
);
```

### start, stop

1. `websocket.start()` starts a background thread that connects to the remote server and processes incoming events.
2. `websocket.stop()` requests closure and waits for that background thread to exit.

For parallel connections, configure a separate `ix::WebSocket` for each URL and
call `start()` on each instance. Each connection attempt runs on its own background
thread, so starting another client does not wait for the first handshake.
`connect()` is the synchronous connection API.

### Threading and callbacks

`setOnMessageCallback()` handles Open, Message, Close, Error, Ping, Pong, and Fragment
events. Callbacks execute inline on the thread processing the connection; they are
not dispatched to an application event loop or the main thread.

| Connection mode | Thread executing event callbacks |
| --- | --- |
| Client using `start()` | The client's background connection thread, including connection attempts and automatic reconnections. |
| Client calling `connect()` explicitly | A successful connection invokes Open on the thread calling `connect()`. A failed connection is returned as a `WebSocketInitResult`. |
| Client calling `run()` explicitly | The thread calling `run()` processes subsequent events. Call `connect()` first, and do not combine this mode with `start()`. |
| `WebSocketServer` | Each connection has its own worker thread. The connection setup callback and that connection's event callbacks run on this thread. Different connections can invoke application callbacks concurrently. |

For a connection owned by `start()` or `WebSocketServer`, message events and the
terminal Close notification are processed on its connection thread. A fatal socket
write failure makes `send()` return an unsuccessful `WebSocketSendInfo` and request
closure without invoking the message handler on the sending thread. The connection
thread finishes any active message callback before delivering Close. Sending on an already closed
connection also returns failure.

Other callbacks have different execution contexts:

| Callback | Execution thread |
| --- | --- |
| Progress callback passed to `send()` | The thread calling `send()`, during the send operation. |
| Traffic tracker registered with `setTrafficTrackerCallback()` | The sending thread for outgoing traffic and the connection thread for incoming traffic. This callback is shared across WebSocket instances and can be called concurrently. |

Keep event handlers short: a slow handler delays incoming messages, heartbeats,
and close processing for that connection. To update a UI or perform lengthy work,
copy the required data into an application queue and process it on the appropriate
thread. Protect application state shared between callbacks and other threads.

Register callbacks before starting the connection and leave them installed until
processing has stopped. Do not replace a callback concurrently with its execution.
Use only one owner to manage `start()`, `stop()`, and destruction; do not invoke
these lifecycle operations concurrently.

To disconnect from inside a message callback, call `close()`. Do not call `stop()`
or destroy the WebSocket there: `stop()` joins the background thread created by
`start()`, and server workers must return from their callbacks to finish shutdown.
Call `stop()` from an owning application thread, and keep the WebSocket and any
objects captured by its callbacks alive until processing has finished. Do not hold
an application mutex across `stop()` if a callback needs that mutex, because
`stop()` waits for the callback's thread to finish. Similarly, stop a server from
outside its connection callbacks.

### Configuring the remote url

The url can be set and queried after a websocket object has been created. You will have to call `stop` and `start` if you want to disconnect and connect to that new url.

```cpp
std::string url("wss://example.com");
websocket.configure(url);
```

### Ping/Pong support

Ping/pong messages are used to implement keep-alive. 2 message types exists to identify ping and pong messages. Note that when a ping message is received, a pong is instantly send back as requested by the WebSocket spec.

```cpp
webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr& msg)
    {
        if (msg->type == ix::WebSocketMessageType::Ping ||
            msg->type == ix::WebSocketMessageType::Pong)
        {
            std::cout << "pong data: " << msg->str << std::endl;
        }
    }
);
```

A ping message can be sent to the server, with an optional data string.

```cpp
websocket.ping("ping data, optional (empty string is ok): limited to 125 bytes long");
```

### Heartbeat.

You can configure an optional heart beat / keep-alive, sent every 45 seconds
when there is no any traffic to make sure that load balancers do not kill an
idle connection.

```cpp
webSocket.setPingInterval(45);
```

### Supply extra HTTP headers.

You can set extra HTTP headers to be sent during the WebSocket handshake.

```cpp
WebSocketHttpHeaders headers;
headers["foo"] = "bar";
webSocket.setExtraHeaders(headers);
```

To omit automatically generated `User-Agent` or `Origin` headers, set their names
explicitly:

```cpp
webSocket.setDisabledDefaultHeaders({"User-Agent", "Origin"});
```

Names are case-insensitive. Only these optional defaults can be disabled; required
handshake headers and other names are unaffected. Headers supplied through
`setExtraHeaders()` always take precedence and are sent even when their values
are empty. Pass an empty list to restore the defaults:

```cpp
webSocket.setDisabledDefaultHeaders({});
```

Changes apply to subsequent connections, including automatic reconnections.

### Subprotocols

You can specify subprotocols to be set during the WebSocket handshake. For more info you can refer to [this doc](https://hpbn.co/websocket/#subprotocol-negotiation).

```cpp
webSocket.addSubprotocol("appProtocol-v1");
webSocket.addSubprotocol("appProtocol-v2");
```

The protocol that the server did accept is available in the open info `protocol` field.

```cpp
std::cout << "protocol: " << msg->openInfo.protocol << std::endl;
```

### Automatic reconnection

Automatic reconnection kicks in when the connection is disconnected without the user consent. This feature is on by default and can be turned off.

```cpp
webSocket.enableAutomaticReconnection();  // turn on
webSocket.disableAutomaticReconnection(); // turn off
bool enabled = webSocket.isAutomaticReconnectionEnabled(); // query state
```

The technique to calculate wait time is called [exponential
backoff](https://docs.aws.amazon.com/general/latest/gr/api-retries.html). Here
are the default waiting times between attempts (from connecting with `ws connect ws://foo.com`)

```
> Connection error: Got bad status connecting to foo.com, status: 301, HTTP Status line: HTTP/1.1 301 Moved Permanently

#retries: 1
Wait time(ms): 100
#retries: 2
Wait time(ms): 200
#retries: 3
Wait time(ms): 400
#retries: 4
Wait time(ms): 800
#retries: 5
Wait time(ms): 1600
#retries: 6
Wait time(ms): 3200
#retries: 7
Wait time(ms): 6400
#retries: 8
Wait time(ms): 10000
```

The waiting time is capped by default at 10s between 2 attempts, but that value
can be changed and queried. The minimum waiting time can also be set.

```cpp
webSocket.setMaxWaitBetweenReconnectionRetries(5 * 1000); // 5000ms = 5s
uint32_t m = webSocket.getMaxWaitBetweenReconnectionRetries();

webSocket.setMinWaitBetweenReconnectionRetries(1000); // 1000ms = 1s
uint32_t m = webSocket.getMinWaitBetweenReconnectionRetries();
```

## Handshake timeout

You can control how long to wait until timing out while waiting for the websocket handshake to be performed.

```
int handshakeTimeoutSecs = 1;
setHandshakeTimeout(handshakeTimeoutSecs);
```

## WebSocket server API

TLS handshakes run in individual connection workers, so an idle peer does not block other clients from connecting. Server TLS handshakes have a 10-second timeout and are cancelled when the server stops. The maximum connection limit includes clients whose TLS handshake is still pending.

### Connection setup callback

`setOnConnectionCallback()` provides a `std::weak_ptr<ix::WebSocket>` and a
`ConnectionState` for each accepted connection. Use this API when the application
needs to retain a handle for later asynchronous sends. Register that connection's
message callback inside the setup callback, as shown below.

Use `connectionState->getId()` as the key in an application registry of weak
pointers. Protect a registry shared by connection callbacks and application threads
with a mutex. To send later, copy the weak pointer while holding the registry lock,
release that lock, then call `lock()` on the weak pointer and send if it succeeds.
An expired weak pointer cannot refer to a later connection, even if its allocation
reuses the same address. Check the send result because the connection can close
after `lock()` succeeds. Capturing a weak pointer also avoids a cycle between the
WebSocket and its own callback.

```cpp
#include <ixwebsocket/IXWebSocketServer.h>

...

// Run a server on localhost at a given port.
// Bound host name, max connections and listen backlog can also be passed in as parameters.
int port = 8008;
std::string host("127.0.0.1"); // If you need this server to be accessible on a different machine, use "0.0.0.0"
ix::WebSocketServer server(port, host);

server.setOnConnectionCallback(
    [&server](std::weak_ptr<ix::WebSocket> webSocket,
              std::shared_ptr<ix::ConnectionState> connectionState)
    {
        std::cout << "Remote ip: " << connectionState->getRemoteIp() << std::endl;

        auto ws = webSocket.lock();
        if (ws)
        {
            ws->setOnMessageCallback(
                [webSocket, connectionState, &server](const ix::WebSocketMessagePtr& msg)
                {
                    if (msg->type == ix::WebSocketMessageType::Open)
                    {
                        std::cout << "New connection" << std::endl;

                        // A connection state object is available, and has a default id
                        // You can subclass ConnectionState and pass an alternate factory
                        // to override it. It is useful if you want to store custom
                        // attributes per connection (authenticated bool flag, attributes, etc...)
                        std::cout << "id: " << connectionState->getId() << std::endl;

                        // The uri the client did connect to.
                        std::cout << "Uri: " << msg->openInfo.uri << std::endl;

                        std::cout << "Headers:" << std::endl;
                        for (auto it : msg->openInfo.headers)
                        {
                            std::cout << it.first << ": " << it.second << std::endl;
                        }
                    }
                    else if (msg->type == ix::WebSocketMessageType::Message)
                    {
                        // For an echo server, we just send back to the client whatever was received by the server
                        // All connected clients are available in an std::set. See the broadcast cpp example.
                        // Second parameter tells whether we are sending the message in binary or text mode.
                        // Here we send it in the same mode as it was received.
                        auto ws = webSocket.lock();
                        if (ws)
                        {
                            ws->send(msg->str, msg->binary);
                        }
                    }
                }
            );
        }
    }
);

auto res = server.listen();
if (!res.first)
{
    // Error handling
    return 1;
}

// Per message deflate connection is enabled by default. It can be disabled
// which might be helpful when running on low power devices such as a Rasbery Pi
server.disablePerMessageDeflate();

// Run the server in the background. Server can be stoped by calling server.stop()
server.start();

// Block until server.stop() is called.
server.wait();

```

### Client message callback

`setOnClientMessageCallback()` installs one handler for all connections. Its
WebSocket reference is valid for the duration of the callback. Use the connection
setup callback above when a weak handle must outlive an individual callback.

```cpp
#include <ixwebsocket/IXWebSocketServer.h>

...

// Run a server on localhost at a given port.
// Bound host name, max connections and listen backlog can also be passed in as parameters.
int port = 8008;
std::string host("127.0.0.1"); // If you need this server to be accessible on a different machine, use "0.0.0.0"
ix::WebSocketServer server(port, host);

server.setOnClientMessageCallback([](std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket & webSocket, const ix::WebSocketMessagePtr & msg) {
    // The ConnectionState object contains information about the connection,
    // at this point only the client ip address and the port.
    std::cout << "Remote ip: " << connectionState->getRemoteIp() << std::endl;

    if (msg->type == ix::WebSocketMessageType::Open)
    {
        std::cout << "New connection" << std::endl;

        // A connection state object is available, and has a default id
        // You can subclass ConnectionState and pass an alternate factory
        // to override it. It is useful if you want to store custom
        // attributes per connection (authenticated bool flag, attributes, etc...)
        std::cout << "id: " << connectionState->getId() << std::endl;

        // The uri the client did connect to.
        std::cout << "Uri: " << msg->openInfo.uri << std::endl;

        std::cout << "Headers:" << std::endl;
        for (auto it : msg->openInfo.headers)
        {
            std::cout << "\t" << it.first << ": " << it.second << std::endl;
        }
    }
    else if (msg->type == ix::WebSocketMessageType::Message)
    {
        // For an echo server, we just send back to the client whatever was received by the server
        // All connected clients are available in an std::set. See the broadcast cpp example.
        // Second parameter tells whether we are sending the message in binary or text mode.
        // Here we send it in the same mode as it was received.
        std::cout << "Received: " << msg->str << std::endl;

        webSocket.send(msg->str, msg->binary);
    }
});

auto res = server.listen();
if (!res.first)
{
    // Error handling
    return 1;
}

// Per message deflate connection is enabled by default. It can be disabled
// which might be helpful when running on low power devices such as a Rasbery Pi
server.disablePerMessageDeflate();

// Run the server in the background. Server can be stoped by calling server.stop()
server.start();

// Block until server.stop() is called.
server.wait();

```

### Heartbeat

You can configure an optional heartbeat / keep-alive for the WebSocket server. The heartbeat interval can be adjusted or disabled when constructing the `WebSocketServer`. Setting the interval to `-1` disables the heartbeat feature; this is the default setting. The parameter you set will be applied to every `WebSocket` object that the server creates.

To enable a 45 second heartbeat on a `WebSocketServer`:

```cpp
int pingIntervalSeconds = 45;
ix::WebSocketServer server(port, host, backlog, maxConnections, handshakeTimeoutSecs, addressFamily, pingIntervalSeconds);
```

### Server log callback

By default the server writes internal errors to stderr (and some info messages to stdout). Errors that happen before a connection is fully established — for example a failed TLS handshake when a client presents a bad certificate — cannot be reported through `setOnClientMessageCallback`, since no WebSocket object exists yet at that point.

To capture those messages in your application logs, set a log callback. It applies to `WebSocketServer` as well as `HttpServer`. When a callback is set, messages are delivered to it instead of being written to stderr/stdout.

```cpp
server.setLogCallback([](ix::LogLevel level, const std::string& msg) {
    if (level == ix::LogLevel::Error)
    {
        myLogger.error(msg);
    }
    else
    {
        myLogger.info(msg);
    }
});
```

## HTTP client API

### Connection reuse

Keep an `ix::HttpClient` instance alive to reuse its most recent HTTP/1.1 connection
for sequential requests to the same scheme, hostname, and port. This also applies
to requests queued through the asynchronous API and avoids repeating TCP/TLS setup.
Only one idle connection is retained; requests remain serialized rather than pipelined.

The client consumes the complete response before reusing a connection, including
chunked trailers and same-origin redirect bodies. `Connection: close` in either
request or response prevents retention. To close the connection after a request:

```cpp
auto args = client.createRequest();
args->extraHeaders["Connection"] = "close";
auto response = client.get(url, args);
```

Calling `setTLSOptions()` or `setProxyOptions()` discards the cached connection.
Configure them before issuing requests. Canceled, malformed,
and incomplete transfers are closed. If an idle connection closes while a new
request is starting, a bodyless GET or HEAD can be retried once before any response
bytes arrive. Uploads and other methods are never automatically replayed after a
transport failure; applications must decide whether retrying is appropriate.

The built-in `HttpServer` still serves one ordinary HTTP request per connection and
sends `Connection: close`. Reuse needs a server that supports persistent HTTP.
### Upgrading an authenticated HTTP connection to WebSocket

`webSocket.connect(httpClient, timeoutSecs)` upgrades the HTTP client's idle
connection in place. This supports servers that require HTTP authentication and
the WebSocket upgrade on the same TCP/TLS connection:

```cpp
ix::HttpClient httpClient;
// Configure TLS and proxy options on httpClient before the HTTP requests.
auto args = httpClient.createRequest();
auto auth = httpClient.post("https://example.com/login", "credentials", args);
if (auth->errorCode != ix::HttpErrorCode::Ok || auth->statusCode != 200)
{
    // Handle authentication failure.
    return;
}

ix::WebSocket webSocket;
webSocket.setUrl("wss://example.com/events");
webSocket.setExtraHeaders({{"Authorization", "Bearer " + auth->body}});
webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr& message)
{
    // Open runs on the connecting thread; messages run on the receiving thread.
});

auto upgraded = webSocket.connect(httpClient, 5);
if (!upgraded.success)
{
    // No replacement connection or redirect is attempted. Authenticate again if needed.
    return;
}
webSocket.start(); // Receive on the connection that was just upgraded.
// ...
webSocket.stop();
```

The WebSocket must be stopped and closed, with a callback registered. Its `ws://`
or `wss://` URL must match the HTTP connection's hostname, effective port, and TLS
mode (`http` to `ws`, `https` to `wss`). The server must support persistent HTTP;
a response with `Connection: close` leaves no connection available for handoff.
An active HTTP request makes the upgrade fail immediately. An unavailable, stale,
or mismatched connection also fails instead of silently opening a new one.

Ownership moves to `WebSocket`, including TLS state and buffered input. Destroying
`HttpClient` afterward leaves the WebSocket intact; later HTTP requests use a new
connection. TLS and proxy settings are inherited from the HTTP client. Other
WebSocket options, including extra headers, compression, subprotocols, and message
limits, apply normally. Authentication headers and cookies are supplied explicitly
with `setExtraHeaders()`; they are not copied automatically from HTTP requests.

Once the connection is handed off, automatic reconnection is disabled, including
when the upgrade is rejected. To repeat connection-bound authentication, stop the
WebSocket, authenticate through HTTP again, and repeat the handoff. Redirects and
failed handshakes close the handed-off connection. The HTTP client is not retained.
Call `start()` only after a successful handoff; on a closed WebSocket, `start()`
remains the normal entry point for establishing a connection.
Normal `webSocket.start()` continues to establish and upgrade a single connection
without involving `HttpClient`.

### Making HTTP requests

```cpp
#include <ixwebsocket/IXHttpClient.h>

...

//
// Preparation
//
HttpClient httpClient;
HttpRequestArgsPtr args = httpClient.createRequest();

// Custom headers can be set
WebSocketHttpHeaders headers;
headers["Foo"] = "bar";
args->extraHeaders = headers;

// Timeout options
args->connectTimeout = connectTimeout;
args->transferTimeout = transferTimeout;

// Redirect options
args->followRedirects = followRedirects;
args->maxRedirects = maxRedirects;

// Misc
args->compress = compress; // Enable gzip compression
args->verbose = verbose;
args->logger = [](const std::string& msg)
{
    std::cout << msg;
};

//
// Synchronous Request
//
HttpResponsePtr out;
std::string url = "https://www.google.com";

// HEAD request
out = httpClient.head(url, args);

// GET request
out = httpClient.get(url, args);

// POST request with parameters
HttpParameters httpParameters;
httpParameters["foo"] = "bar";

// HTTP form data can be passed in as well, for multi-part upload of files
HttpFormDataParameters httpFormDataParameters;
httpParameters["baz"] = "booz";

out = httpClient.post(url, httpParameters, httpFormDataParameters, args);

// POST request with a body
out = httpClient.post(url, std::string("foo=bar"), args);

// PUT and PATCH are available too.

//
// Result
//
auto statusCode = response->statusCode; // Can be HttpErrorCode::Ok, HttpErrorCode::UrlMalformed, etc...
auto errorCode = response->errorCode; // 200, 404, etc...
auto responseHeaders = response->headers; // All the headers in a special case-insensitive unordered_map of (string, string)
auto body = response->body; // All the bytes from the response as an std::string
auto errorMsg = response->errorMsg; // Descriptive error message in case of failure
auto uploadSize = response->uploadSize; // Byte count of uploaded data
auto downloadSize = response->downloadSize; // Byte count of downloaded data

//
// Asynchronous Request
//
bool async = true;
HttpClient httpClient(async);
auto args = httpClient.createRequest(url, HttpClient::kGet);

// If you define a chunk callback it will be called repeteadly with the
// incoming data. This allows to process data on the go or write it to disk
// instead of accumulating the data in memory.
args.onChunkCallback = [](const std::string& data)
{
    // process data
};

// Push the request to a queue,
bool ok = httpClient.performRequest(args, [](const HttpResponsePtr& response)
    {
        // This callback execute in a background thread. Make sure you uses appropriate protection such as mutex
        auto statusCode = response->statusCode; // acess results

        // response->body is empty if onChunkCallback was used
    }
);

// ok will be false if your httpClient is not async

// A request in progress can be cancelled by setting the cancel flag. It does nothing if the request already completed.
args->cancel = true;
```

`performRequest()` copies the request configuration when it is submitted. After
it returns, reusing the arguments for another URL, body, or headers does not change
earlier queued requests. Do not modify the configuration concurrently with the
submission itself. Callbacks are copied, but objects they capture still need to
remain valid when the callbacks run.

The original `args->cancel` flag remains the cancellation handle, including while
following redirects. Reusing an arguments object shares that flag across its
submitted requests; use separate arguments objects for independently cancellable
requests. HTTP connection reuse is not provided by this behavior.

Automatic HTTP and WebSocket redirects retain caller-supplied `Authorization` and `Cookie`
headers only while the scheme, hostname and port stay the same. Once a redirect
changes origin, these headers remain removed for the rest of that redirect chain.
The caller's original request arguments are preserved. Other custom headers are
forwarded; applications should avoid putting credentials in those headers when
following redirects to untrusted destinations.

See this [issue](https://github.com/machinezone/IXWebSocket/issues/209) for links about uploading files with HTTP multipart.

### Uploading large files

Use `HttpRequestArgs::onUploadCallback` to send a request body without loading the
whole file into a string. Set `bodySize` to its known byte length; it is a 64-bit
value, so uploads larger than 2 GiB are supported. The client sends a standard
`Content-Length` request, using a reusable buffer of at most 64 KiB.

```cpp
#include <fstream>
#include <ixwebsocket/IXHttpClient.h>
#include <memory>
#include <stdexcept>

ix::HttpClient client;
auto args = client.createRequest();
auto file = std::make_shared<std::ifstream>("large-file.bin", std::ios::binary | std::ios::ate);
if (!*file || file->tellg() < 0)
{
    throw std::runtime_error("Cannot open upload file");
}
args->bodySize = static_cast<uint64_t>(static_cast<std::streamoff>(file->tellg()));
args->extraHeaders["Content-Type"] = "application/octet-stream";
args->onUploadCallback = [file](uint64_t offset, char* buffer, size_t capacity) -> size_t
{
    file->clear();
    file->seekg(static_cast<std::streamoff>(offset));
    file->read(buffer, static_cast<std::streamsize>(capacity));
    return static_cast<size_t>(file->gcount());
};
auto response = client.put("https://example.com/upload", std::string(), args);
```

The callback receives an absolute body offset and the available buffer capacity.
Return the number of bytes written, at most that capacity. Short reads are allowed;
a zero return before `bodySize` bytes, an invalid count, or an exception fails the
request with `HttpErrorCode::SendError`. Set `args->cancel` to cancel an upload.
Callbacks must return promptly for cancellation and transfer timeouts to take effect.

Pass an empty string as the body when using the callback. It works with POST, PUT,
PATCH, and methods enabled through `setForceBody(true)`, for synchronous requests
and queued requests through `performRequest`. Keep the data source alive until
completion; the shared pointer in this example also works for asynchronous requests.
Redirects replay the body from offset zero, so the source must support rereading.

Do not set `Content-Length` yourself or enable `compressRequest` for streamed
uploads. To use `Transfer-Encoding: chunked`, see below. To upload a multipart
body, the callback must supply its complete framing and a matching `Content-Type`; the existing
form-data helpers still assemble their bodies in memory. This API streams outgoing
client data; the built-in HTTP server still buffers incoming request bodies.

### Chunked request uploads

Set `Transfer-Encoding: chunked` to upload a body whose length is unknown, or when
an endpoint requires chunked requests. With `onUploadCallback`, `bodySize` is
ignored and returning zero finishes the body. Throw an exception on a source read
error; it fails the request with `HttpErrorCode::SendError`.

```cpp
ix::HttpClient client;
auto args = client.createRequest();
auto file = std::make_shared<std::ifstream>("upload.bin", std::ios::binary);
if (!*file)
{
    throw std::runtime_error("Cannot open upload file");
}
args->extraHeaders["Transfer-Encoding"] = "chunked";
args->extraHeaders["Content-Type"] = "application/octet-stream";
args->onUploadCallback = [file](uint64_t offset, char* buffer, size_t capacity) -> size_t
{
    file->clear();
    file->seekg(static_cast<std::streamoff>(offset));
    file->read(buffer, static_cast<std::streamsize>(capacity));
    if (file->bad() || (file->fail() && !file->eof()))
    {
        throw std::runtime_error("Cannot read upload file");
    }
    return static_cast<size_t>(file->gcount());
};
auto response = client.put("https://example.com/upload", std::string(), args);
```

Include the same C++ headers as the file upload example above. The client supplies
chunk framing and the terminating chunk, using data chunks of at most 64 KiB.
You can also set this header with a regular string body. Do not combine it with
`Content-Length`, request compression, or other transfer codings. Existing uploads
continue to use `Content-Length` by default. Cancellation and redirect replay work
as for fixed-length uploads; a 303 redirect switches to GET and drops the body.

For asynchronous uploads, use `HttpClient(true)` and `performRequest` as usual.
`onChunkCallback` receives **response** data; `onResponseCallback` runs once when
the request completes. Each request finishes sending its body before reading its
response; simultaneous request and response streaming is not supported.

The built-in `HttpServer` currently accepts `Content-Length` request bodies and
buffers them in memory; it does not decode chunked request bodies. Use a server
that supports chunked requests as the upload endpoint.

## HTTP server API

```cpp
#include <ixwebsocket/IXHttpServer.h>

ix::HttpServer server(port, hostname);
server.setDocumentRoot("www");

auto res = server.listen();
if (!res.first)
{
    std::cerr << res.second << std::endl;
    return 1;
}

server.start();
server.wait();
```

The built-in static file handler serves the current directory by default. Call
`setDocumentRoot(path)` before starting the server to select a relative or absolute
directory. An empty path selects the current directory. Relative paths are resolved
against the process working directory, and requests for `/` serve `index.html`.
This setting applies to the built-in handler; custom connection callbacks continue
to handle their own responses.

The command-line server provides the same option:

```sh
ws httpd --port 8008 --document-root www
```

If you want to handle how requests are processed, implement the setOnConnectionCallback callback, which takes an HttpRequestPtr as input, and returns an HttpResponsePtr. You can look at HttpServer::setDefaultConnectionCallback for a slightly more advanced callback example.

```cpp
setOnConnectionCallback(
    [this](HttpRequestPtr request,
           std::shared_ptr<ConnectionState> connectionState) -> HttpResponsePtr
    {
        // Build a string for the response
        std::stringstream ss;
        ss << connectionState->getRemoteIp();
           << " "
           << request->method
           << " "
           << request->uri;

        std::string content = ss.str();

        return std::make_shared<HttpResponse>(200, "OK",
                                              HttpErrorCode::Ok,
                                              WebSocketHttpHeaders(),
                                              content);
}
```

## TLS support and configuration

To leverage TLS features, the library must be compiled with the option `USE_TLS=1`.

If you are using OpenSSL, try to be on a version higher than 1.1.x as there there are thread safety problems with 1.0.x.

Then, secure sockets are automatically used when connecting to a `wss://*` url.

Additional TLS options can be configured by passing a `ix::SocketTLSOptions` instance to the
`setTLSOptions` on `ix::WebSocket` (or `ix::WebSocketServer` or `ix::HttpServer`)

A TLS-enabled build still creates a plain server listener by default. Enable TLS
on the listener before calling `listen()`:

```cpp
ix::SocketTLSOptions serverTLS;
serverTLS.tls = true;
serverTLS.certFile = "path/to/server-cert.pem";
serverTLS.keyFile = "path/to/server-key.pem";
server.setTLSOptions(serverTLS);
```

Clients select TLS with a `wss://` URL. For a local server signed by a private CA,
configure the matching trust bundle before starting the client:

```cpp
ix::SocketTLSOptions clientTLS;
clientTLS.caFile = "path/to/ca.pem";
webSocket.setTLSOptions(clientTLS);
webSocket.setUrl("wss://localhost:8008");
```

Specifying `certFile` and `keyFile` configures the certificate that will be used to communicate with TLS peers.

On a client, this is only necessary for connecting to servers that require a client certificate.

With OpenSSL, mbedTLS, and Network.framework, `certFile` and `keyFile` can each
contain either a file path or the PEM contents directly, just like `caFile`:

```cpp
ix::SocketTLSOptions options;
options.certFile = certificateChainPEM; // Leaf certificate first, then intermediates.
options.keyFile = privateKeyPEM;        // Matching, unencrypted PEM private key.
options.caFile = trustedRootsPEM;       // Or a CA file path / "SYSTEM".
webSocket.setTLSOptions(options);
```

The same options work for TLS servers (`options.tls = true`). File paths and
in-memory contents can be mixed; no temporary credential files are created.
Unencrypted PKCS#8, RSA, and EC PEM keys are supported. Encrypted private keys
have no password API and are not supported. `getDescription()` redacts in-memory
PEM contents. SecureTransport does not support in-memory local credentials;
use Network.framework on macOS for this feature.

On a server, this is necessary for TLS support.

Specifying `caFile` configures the trusted roots bundle file (in PEM format) that will be used to verify peer certificates.
 - The special value of `SYSTEM` (the default) indicates that the system-configured trust bundle should be used; this is generally what you want when connecting to any publicly exposed API/server.
 - The special value of `NONE` can be used to disable peer verification; this is only recommended to rule out certificate verification when testing connectivity.
 - If the value contain the special value `-----BEGIN CERTIFICATE-----`, the value will be read from memory, and not from a file. This is convenient on platforms like Android where reading / writing to the file system can be challenging without proper permissions, or without knowing the location of a temp directory.

The Mbed TLS backend currently supports `caFile = "SYSTEM"` only on Windows.
On other platforms, including macOS, set `caFile` explicitly to the path of a
trusted PEM CA bundle or its PEM contents, including for publicly trusted servers.
Otherwise the connection fails with an error explaining the missing configuration.
Use a bundle containing a trusted root for the server's certificate chain; keep
hostname and certificate verification enabled.

For a client, specifying `caFile` can be used if connecting to a server that uses a self-signed cert, or when using a custom CA in an internal environment.

For a server, specifying `caFile` implies that:
1. You require clients to present a certificate
1. It must be signed by one of the trusted roots in the file

By default, a destination's hostname is always validated against the certificate that it presents. To accept certificates with any hostname, set `ix::SocketTLSOptions::disable_hostname_validation` to `true`.

### TLS key logging (OpenSSL)

With OpenSSL 1.1.1 or newer, set `keyLogFile` in your TLS options before connecting
or starting a TLS server:

```cpp
ix::SocketTLSOptions options;
options.keyLogFile = "tls.keys";
webSocket.setTLSOptions(options);
```

The file contains TLS session secrets in NSS key-log format, which tools such as
Wireshark can use to decrypt captured traffic. An empty `keyLogFile` disables this
logging. Connections append to the file, including across reconnections; multiple
connections may share a destination. A connection fails if the file cannot be
opened. Other TLS backends reject this option explicitly.

The `ws` TLS commands accept `--keylog-file` and the `SSLKEYLOGFILE` environment
variable. An explicit command-line option takes precedence:

```sh
ws connect wss://example.com/ws --keylog-file tls.keys
SSLKEYLOGFILE=tls.keys ws connect wss://example.com/ws
```

The library API uses `keyLogFile` explicitly; environment-variable handling is
provided by the CLI.

### Network.framework backend on macOS

Build with `USE_NETWORK_FRAMEWORK=ON` to use native TLS for `wss://` and `https://`
clients and TLS-enabled WebSocket/HTTP servers. Network.framework handles connection
setup, DNS, and TCP. Application HTTP and WebSocket handling stays in IXWebSocket.
TLS 1.2 is the minimum; TLS 1.3 is negotiated when supported by the peer.

The existing `SocketTLSOptions` fields apply:

- `certFile` and `keyFile` accept a PEM certificate chain (leaf first) and matching,
  unencrypted PEM private key. Both are required for a TLS server and optional for a
  client presenting a certificate. Credentials are imported in memory, without
  installing them in a keychain.
- `caFile = "SYSTEM"` uses Apple's trust evaluation and configured system trust.
  A PEM CA file or in-memory PEM bundle instead restricts trust to those anchors.
- `caFile = "NONE"` disables peer verification. On a server this also disables the
  requirement for a client certificate. Other CA settings require client
  authentication, matching the existing server options.
- `disable_hostname_validation` skips the client's hostname check while retaining
  certificate trust evaluation. IP literals are checked against IP SAN entries.
- Custom `ciphers` strings are currently rejected; leave `ciphers` at `DEFAULT`.

The native server uses a Network.framework listener. `createSocket` cannot adopt an
existing BSD descriptor for TLS with this backend. The listener requires a numeric
bind address and uses the framework's TCP listen queue rather than the BSD backlog
setting. Pending connections are bounded by `maxConnections`. Server TLS handshakes
have a ten-second limit and are interrupted when the server stops. Client connection
timeouts use the caller's cancellation callback, including HTTP/WebSocket timeouts.


### SOCKS5 client proxies

Configure an outbound SOCKS5 proxy before connecting a WebSocket or issuing HTTP
requests. The same options apply to HTTP, HTTPS, WS, and WSS:

```cpp
ix::ProxyOptions proxy;
proxy.host = "127.0.0.1";
proxy.port = 1080; // default
proxy.dnsMode = ix::ProxyOptions::DnsMode::Remote; // default: proxy resolves the destination

webSocket.setProxyOptions(proxy);
httpClient.setProxyOptions(proxy);
```

Use `DnsMode::Local` to resolve the destination locally and send an IPv4 or IPv6
address to the proxy. Numeric destinations are always sent as numeric addresses.
The proxy's own hostname is resolved locally in both modes. TLS continues to use
the original destination hostname for SNI and certificate verification.

Set both `proxy.username` and `proxy.password` to offer SOCKS username/password
authentication as well as the unauthenticated method. Each credential must contain
1–255 bytes. Proxy authentication takes place before the destination TLS handshake.
An empty proxy host disables this proxy configuration; call `setProxyOptions({})`
to clear it. Configure options before use; WebSocket reconnections use the current
configuration. Proxy negotiation shares the existing connection timeout and
cancellation mechanism, and failures do not fall back to a direct connection.

SOCKS5 CONNECT is supported for plain sockets, OpenSSL/LibreSSL, Mbed TLS, and
SecureTransport. SOCKS4 and HTTP CONNECT proxies are not implemented. The native
Network.framework TLS backend currently returns an explicit unsupported-proxy
error. Its native SOCKS API failed routing and fragmented-reply checks during
implementation, so it is not enabled by this feature.
