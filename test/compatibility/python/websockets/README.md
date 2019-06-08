# Clients

## ws

```
$ ws connect ws://127.0.0.1:8765
Type Ctrl-D to exit prompt...
Connecting to url: ws://127.0.0.1:8765
> ws_connect: connected
Uri: /
Handshake Headers:
Connection: Upgrade
Date: Sat, 08 Jun 2019 16:43:29 GMT
Sec-WebSocket-Accept: kPCNwGa97y+7NWdAvHi/7/rA8AE=
Sec-WebSocket-Extensions: permessage-deflate; server_max_window_bits=15; client_max_window_bits=15
Server: Python/3.7 websockets/7.0
Upgrade: websocket
Received 13 bytes
ws_connect: received message: > Welcome !
ws_connect: connection closed: code 1006 reason Abnormal closure
```

## wscat

```
$ ./node_modules/.bin/wscat -c ws://127.0.0.1:8765
connected (press CTRL+C to quit)
< > Welcome !
disconnected (code: 1006)
```
