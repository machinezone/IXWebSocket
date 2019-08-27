## Examples

The [*ws*](https://github.com/machinezone/IXWebSocket/tree/master/ws) folder countains many interactive programs for chat, [file transfers](https://github.com/machinezone/IXWebSocket/blob/master/ws/ws_send.cpp), [curl like](https://github.com/machinezone/IXWebSocket/blob/master/ws/ws_http_client.cpp) http clients, demonstrating client and server usage.

### Windows note

To use the network system on Windows, you need to initialize it once with *WSAStartup()* and clean it up with *WSACleanup()*. We have helpers for that which you can use, see below. This init would typically take place in your main function.

```
#include <ixwebsocket/IXNetSystem.h>

int main()
{
    ix::initNetSystem();

    ...

    ix::uninitNetSystem();
    return 0;
}
```

### WebSocket client API

```
#include <ixwebsocket/IXWebSocket.h>

...

# Our websocket object
ix::WebSocket webSocket;

std::string url("ws://localhost:8080/");
webSocket.setUrl(url);

// Optional heart beat, sent every 45 seconds when there is not any traffic
// to make sure that load balancers do not kill an idle connection.
webSocket.setHeartBeatPeriod(45);

// Per message deflate connection is enabled by default. You can tweak its parameters or disable it
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

### WebSocket server API

```
#include <ixwebsocket/IXWebSocketServer.h>

...

// Run a server on localhost at a given port.
// Bound host name, max connections and listen backlog can also be passed in as parameters.
ix::WebSocketServer server(port);

server.setOnConnectionCallback(
    [&server](std::shared_ptr<WebSocket> webSocket,
              std::shared_ptr<ConnectionState> connectionState)
    {
        webSocket->setOnMessageCallback(
            [webSocket, connectionState, &server](const ix::WebSocketMessagePtr msg)
            {
                if (msg->type == ix::WebSocketMessageType::Open)
                {
                    std::cerr << "New connection" << std::endl;

                    // A connection state object is available, and has a default id
                    // You can subclass ConnectionState and pass an alternate factory
                    // to override it. It is useful if you want to store custom
                    // attributes per connection (authenticated bool flag, attributes, etc...)
                    std::cerr << "id: " << connectionState->getId() << std::endl;

                    // The uri the client did connect to.
                    std::cerr << "Uri: " << msg->openInfo.uri << std::endl;

                    std::cerr << "Headers:" << std::endl;
                    for (auto it : msg->openInfo.headers)
                    {
                        std::cerr << it.first << ": " << it.second << std::endl;
                    }
                }
                else if (msg->type == ix::WebSocketMessageType::Message)
                {
                    // For an echo server, we just send back to the client whatever was received by the server
                    // All connected clients are available in an std::set. See the broadcast cpp example.
                    // Second parameter tells whether we are sending the message in binary or text mode.
                    // Here we send it in the same mode as it was received.
                    webSocket->send(msg->str, msg->binary);
                }
            }
        );
    }
);

auto res = server.listen();
if (!res.first)
{
    // Error handling
    return 1;
}

// Run the server in the background. Server can be stoped by calling server.stop()
server.start();

// Block until server.stop() is called.
server.wait();

```

### HTTP client API

```
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
out = httpClient.post(url, httpParameters, args);

// POST request with a body
out = httpClient.post(url, std::string("foo=bar"), args);

//
// Result
//
auto statusCode = response->statusCode; // Can be HttpErrorCode::Ok, HttpErrorCode::UrlMalformed, etc...
auto errorCode = response->errorCode; // 200, 404, etc...
auto responseHeaders = response->headers; // All the headers in a special case-insensitive unordered_map of (string, string)
auto payload = response->payload; // All the bytes from the response as an std::string
auto errorMsg = response->errorMsg; // Descriptive error message in case of failure
auto uploadSize = response->uploadSize; // Byte count of uploaded data
auto downloadSize = response->downloadSize; // Byte count of downloaded data

//
// Asynchronous Request
//
bool async = true;
HttpClient httpClient(async);
auto args = httpClient.createRequest(url, HttpClient::kGet);

// Push the request to a queue,
bool ok = httpClient.performRequest(args, [](const HttpResponsePtr& response)
    {
        // This callback execute in a background thread. Make sure you uses appropriate protection such as mutex
        auto statusCode = response->statusCode; // acess results
    }
);

// ok will be false if your httpClient is not async
```

### HTTP server API

```
#include <ixwebsocket/IXHttpServer.h>

ix::HttpServer server(port, hostname);

auto res = server.listen();
if (!res.first)
{
    std::cerr << res.second << std::endl;
    return 1;
}

server.start();
server.wait();
```

If you want to handle how requests are processed, implement the setOnConnectionCallback callback, which takes an HttpRequestPtr as input, and returns an HttpResponsePtr. You can look at HttpServer::setDefaultConnectionCallback for a slightly more advanced callback example.

```
setOnConnectionCallback(
    [this](HttpRequestPtr request,
           std::shared_ptr<ConnectionState> /*connectionState*/) -> HttpResponsePtr
    {
        // Build a string for the response
        std::stringstream ss;
        ss << request->method
           << " "
           << request->uri;

        std::string content = ss.str();

        return std::make_shared<HttpResponse>(200, "OK",
                                              HttpErrorCode::Ok,
                                              WebSocketHttpHeaders(),
                                              content);
}
```
