## General

[cobra](https://github.com/machinezone/cobra) is a real time messaging server. The `ws` utility can run a cobra server (named snake), and has client to publish and subscribe to a cobra server.

Bring up 3 terminals and run a server, a publisher and a subscriber in each one. As you publish data you should see it being received by the subscriber. You can run `redis-cli MONITOR` too to see how redis is being used.

### Server

You will need to have a redis server running locally. To run the server:

```bash
$ cd <ixwebsocket-top-level-folder>/ixsnake/ixsnake
$ ws snake
{
  "apps": {
    "FC2F10139A2BAc53BB72D9db967b024f": {
      "roles": {
        "_sub": {
          "secret": "66B1dA3ED5fA074EB5AE84Dd8CE3b5ba"
        },
        "_pub": {
          "secret": "1c04DB8fFe76A4EeFE3E318C72d771db"
        }
      }
    }
  }
}

redis host: 127.0.0.1
redis password:
redis port: 6379
```

### Publisher

```bash
$ cd <ixwebsocket-top-level-folder>/ws
$ ws cobra_publish --appkey FC2F10139A2BAc53BB72D9db967b024f --endpoint ws://127.0.0.1:8008 --rolename _pub --rolesecret 1c04DB8fFe76A4EeFE3E318C72d771db test_channel cobraMetricsSample.json
[2019-11-27 09:06:12.980] [info] Publisher connected
[2019-11-27 09:06:12.980] [info] Connection: Upgrade
[2019-11-27 09:06:12.980] [info] Sec-WebSocket-Accept: zTtQKMKbvwjdivURplYXwCVUCWM=
[2019-11-27 09:06:12.980] [info] Sec-WebSocket-Extensions: permessage-deflate; server_max_window_bits=15; client_max_window_bits=15
[2019-11-27 09:06:12.980] [info] Server: ixwebsocket/7.4.0 macos ssl/DarwinSSL zlib 1.2.11
[2019-11-27 09:06:12.980] [info] Upgrade: websocket
[2019-11-27 09:06:12.982] [info] Publisher authenticated
[2019-11-27 09:06:12.982] [info] Published msg 3
[2019-11-27 09:06:12.982] [info] Published message id 3 acked
```

### Subscriber

```bash
$ ws cobra_subscribe --appkey FC2F10139A2BAc53BB72D9db967b024f --endpoint ws://127.0.0.1:8008 --rolename _pub --rolesecret 1c04DB8fFe76A4EeFE3E318C72d771db test_channel
#messages 0 msg/s 0
[2019-11-27 09:07:39.341] [info] Subscriber connected
[2019-11-27 09:07:39.341] [info] Connection: Upgrade
[2019-11-27 09:07:39.341] [info] Sec-WebSocket-Accept: 9vkQWofz49qMCUlTSptCCwHWm+Q=
[2019-11-27 09:07:39.341] [info] Sec-WebSocket-Extensions: permessage-deflate; server_max_window_bits=15; client_max_window_bits=15
[2019-11-27 09:07:39.341] [info] Server: ixwebsocket/7.4.0 macos ssl/DarwinSSL zlib 1.2.11
[2019-11-27 09:07:39.341] [info] Upgrade: websocket
[2019-11-27 09:07:39.342] [info] Subscriber authenticated
[2019-11-27 09:07:39.345] [info] Subscriber: subscribed to channel test_channel
#messages 0 msg/s 0
#messages 0 msg/s 0
#messages 0 msg/s 0
{"baz":123,"foo":"bar"}

#messages 1 msg/s 1
#messages 1 msg/s 0
#messages 1 msg/s 0
{"baz":123,"foo":"bar"}

{"baz":123,"foo":"bar"}

#messages 3 msg/s 2
#messages 3 msg/s 0
{"baz":123,"foo":"bar"}

#messages 4 msg/s 1
^C
```
