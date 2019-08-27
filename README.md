## API

### Sending messages

`websocket.send("foo")` will send a message.

If the connection was closed and sending failed, the return value will be set to false.

### ReadyState

`getReadyState()` returns the state of the connection. There are 4 possible states.

1. ReadyState::Connecting - The connection is not yet open.
2. ReadyState::Open       - The connection is open and ready to communicate.
3. ReadyState::Closing    - The connection is in the process of closing.
4. ReadyState::Closed     - The connection is closed or could not be opened.

### Open and Close notifications

The onMessage event will be fired when the connection is opened or closed. This is similar to the [Javascript browser API](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket), which has `open` and `close` events notification that can be registered with the browser `addEventListener`.

```
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

```
webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr& msg)
    {
        if (msg->type == ix::WebSocketMessageType::Error)
        {
            std::stringstream ss;
            ss << "Error: "         << msg->errorInfo.reason      << std::endl;
            ss << "#retries: "      << msg->eventInfo.retries     << std::endl;
            ss << "Wait time(ms): " << msg->eventInfo.wait_time   << std::endl;
            ss << "HTTP Status: "   << msg->eventInfo.http_status << std::endl;
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

```
websocket.ping("ping data, optional (empty string is ok): limited to 125 bytes long");
```

### Heartbeat.

You can configure an optional heart beat / keep-alive, sent every 45 seconds
when there is no any traffic to make sure that load balancers do not kill an
idle connection.

```
webSocket.setHeartBeatPeriod(45);
```

### Supply extra HTTP headers.

You can set extra HTTP headers to be sent during the WebSocket handshake.

```
WebSocketHttpHeaders headers;
headers["foo"] = "bar";
webSocket.setExtraHeaders(headers);
```
