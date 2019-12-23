## General

ws is a command line tool that should exercise most of the IXWebSocket code, and provide example code.

```
ws is a websocket tool
Usage: ws [OPTIONS] SUBCOMMAND

Options:
  -h,--help                   Print this help message and exit

Subcommands:
  send                        Send a file
  receive                     Receive a file
  transfer                    Broadcasting server
  connect                     Connect to a remote server
  chat                        Group chat
  echo_server                 Echo server
  broadcast_server            Broadcasting server
  ping                        Ping pong
  curl                        HTTP Client
  redis_publish               Redis publisher
  redis_subscribe             Redis subscriber
  cobra_subscribe             Cobra subscriber
  cobra_publish               Cobra publisher
  cobra_to_statsd             Cobra to statsd
  cobra_to_sentry             Cobra to sentry
  snake                       Snake server
  httpd                       HTTP server
```

## curl

The curl subcommand try to be compatible with the curl syntax, to fetch http pages.

Making a HEAD request with the -I parameter.

```
$ ws curl -I https://www.google.com/

Accept-Ranges: none
Alt-Svc: quic=":443"; ma=2592000; v="46,43",h3-Q048=":443"; ma=2592000,h3-Q046=":443"; ma=2592000,h3-Q043=":443"; ma=2592000
Cache-Control: private, max-age=0
Content-Type: text/html; charset=ISO-8859-1
Date: Tue, 08 Oct 2019 21:36:57 GMT
Expires: -1
P3P: CP="This is not a P3P policy! See g.co/p3phelp for more info."
Server: gws
Set-Cookie: NID=188=ASwfz8GrXQrHCLqAz-AndLOMLcz0rC9yecnf3h0yXZxRL3rTufTU_GDDwERp7qQL7LZ_EB8gCRyPXGERyOSAgaqgnrkoTmvWrwFemRLMaOZ896GrHobi5fV7VLklnSG2w48Gj8xMlwxfP7Z-bX-xR9UZxep1tHM6UmFQdD_GkBE; expires=Wed, 08-Apr-2020 21:36:57 GMT; path=/; domain=.google.com; HttpOnly
Transfer-Encoding: chunked
Vary: Accept-Encoding
X-Frame-Options: SAMEORIGIN
X-XSS-Protection: 0
Upload size: 143
Download size: 0
Status: 200
```

Making a POST request with the -F parameter.

```
$ ws curl -F foo=bar https://httpbin.org/post
foo: bar
Downloaded 438 bytes out of 438
Access-Control-Allow-Credentials: true
Access-Control-Allow-Origin: *
Connection: keep-alive
Content-Encoding:
Content-Length: 438
Content-Type: application/json
Date: Tue, 08 Oct 2019 21:47:54 GMT
Referrer-Policy: no-referrer-when-downgrade
Server: nginx
X-Content-Type-Options: nosniff
X-Frame-Options: DENY
X-XSS-Protection: 1; mode=block
Upload size: 219
Download size: 438
Status: 200
payload: {
  "args": {},
  "data": "",
  "files": {},
  "form": {
    "foo": "bar"
  },
  "headers": {
    "Accept": "*/*",
    "Content-Length": "7",
    "Content-Type": "application/x-www-form-urlencoded",
    "Host": "httpbin.org",
    "User-Agent": "ixwebsocket/7.0.0 macos ssl/OpenSSL OpenSSL 1.0.2q  20 Nov 2018 zlib 1.2.11"
  },
  "json": null,
  "origin": "155.94.127.118, 155.94.127.118",
  "url": "https://httpbin.org/post"
}
```

Passing in a custom header with -H.

```
$ ws curl -F foo=bar -H 'my_custom_header: baz' https://httpbin.org/post
my_custom_header:  baz
foo: bar
Downloaded 470 bytes out of 470
Access-Control-Allow-Credentials: true
Access-Control-Allow-Origin: *
Connection: keep-alive
Content-Encoding:
Content-Length: 470
Content-Type: application/json
Date: Tue, 08 Oct 2019 21:50:25 GMT
Referrer-Policy: no-referrer-when-downgrade
Server: nginx
X-Content-Type-Options: nosniff
X-Frame-Options: DENY
X-XSS-Protection: 1; mode=block
Upload size: 243
Download size: 470
Status: 200
payload: {
  "args": {},
  "data": "",
  "files": {},
  "form": {
    "foo": "bar"
  },
  "headers": {
    "Accept": "*/*",
    "Content-Length": "7",
    "Content-Type": "application/x-www-form-urlencoded",
    "Host": "httpbin.org",
    "My-Custom-Header": "baz",
    "User-Agent": "ixwebsocket/7.0.0 macos ssl/OpenSSL OpenSSL 1.0.2q  20 Nov 2018 zlib 1.2.11"
  },
  "json": null,
  "origin": "155.94.127.118, 155.94.127.118",
  "url": "https://httpbin.org/post"
}
```

## connect

The connect command connects to a websocket endpoint, and starts an interactive prompt. Line editing, such as using the direction keys to fetch the last thing you tried to type) is provided. That command is pretty useful to try to send random data to an endpoint and verify that the service handles it with grace (such as sending invalid json).

```
ws connect wss://echo.websocket.org
Type Ctrl-D to exit prompt...
Connecting to url: wss://echo.websocket.org
> ws_connect: connected
Uri: /
Handshake Headers:
Connection: Upgrade
Date: Tue, 08 Oct 2019 21:38:44 GMT
Sec-WebSocket-Accept: 2j6LBScZveqrMx1W/GJkCWvZo3M=
sec-websocket-extensions:
Server: Kaazing Gateway
Upgrade: websocket
Received ping
Received ping
Received ping
Hello world !
> Received 13 bytes
ws_connect: received message: Hello world !
> Hello world !
> Received 13 bytes
ws_connect: received message: Hello world !
```

```
ws connect 'ws://jeanserge.com/v2?appkey=_pubsub'
Type Ctrl-D to exit prompt...
Connecting to url: ws://jeanserge.com/v2?appkey=_pubsub
> ws_connect: connected
Uri: /v2?appkey=_pubsub
Handshake Headers:
Connection: Upgrade
Date: Tue, 08 Oct 2019 21:45:28 GMT
Sec-WebSocket-Accept: LYHmjh9Gsu/Yw7aumQqyPObOEV4=
Sec-WebSocket-Extensions: permessage-deflate; server_max_window_bits=15; client_max_window_bits=15
Server: Python/3.7 websockets/8.0.2
Upgrade: websocket
bababababababab
> ws_connect: connection closed: code 1000 reason

ws_connect: connected
Uri: /v2?appkey=_pubsub
Handshake Headers:
Connection: Upgrade
Date: Tue, 08 Oct 2019 21:45:44 GMT
Sec-WebSocket-Accept: I1rqxdLgTU+opPi5/zKPBTuXdLw=
Sec-WebSocket-Extensions: permessage-deflate; server_max_window_bits=15; client_max_window_bits=15
Server: Python/3.7 websockets/8.0.2
Upgrade: websocket
```

## Websocket proxy

```
ws proxy_server --remote_host ws://127.0.0.1:9000 -v
Listening on 127.0.0.1:8008
```

If you connect to ws://127.0.0.1:8008, the proxy will connect to ws://127.0.0.1:9000 and pass all traffic to this server.

## File transfer

```
# Start transfer server, which is just a broadcast server at this point
ws transfer # running on port 8080.

# Start receiver first
ws receive ws://localhost:8080

# Then send a file. File will be received and written to disk by the receiver process
ws send ws://localhost:8080 /file/to/path
```

## HTTP Client

```
$ ws curl --help
HTTP Client
Usage: ws curl [OPTIONS] url

Positionals:
  url TEXT REQUIRED           Connection url

Options:
  -h,--help                   Print this help message and exit
  -d TEXT                     Form data
  -F TEXT                     Form data
  -H TEXT                     Header
  --output TEXT               Output file
  -I                          Send a HEAD request
  -L                          Follow redirects
  --max-redirects INT         Max Redirects
  -v                          Verbose
  -O                          Save output to disk
  --compress                  Enable gzip compression
  --connect-timeout INT       Connection timeout
  --transfer-timeout INT      Transfer timeout
```

## Cobra client and server

[cobra](https://github.com/machinezone/cobra) is a real time messenging server. ws has several sub-command to interact with cobra. There is also a minimal cobra compatible server named snake available.

Below are examples on running a snake server and clients with TLS enabled (the server only works with the OpenSSL and the Mbed TLS backend for now).

First, generate certificates.

```
$ cd /path/to/IXWebSocket
$ cd ixsnake/ixsnake
$ bash ../../ws/generate_certs.sh
Generating RSA private key, 2048 bit long modulus
.....+++
.................+++
e is 65537 (0x10001)
generated ./.certs/trusted-ca-key.pem
generated ./.certs/trusted-ca-crt.pem
Generating RSA private key, 2048 bit long modulus
..+++
.......................................+++
e is 65537 (0x10001)
generated ./.certs/trusted-server-key.pem
Signature ok
subject=/O=machinezone/O=IXWebSocket/CN=trusted-server
Getting CA Private Key
generated ./.certs/trusted-server-crt.pem
Generating RSA private key, 2048 bit long modulus
...................................+++
..................................................+++
e is 65537 (0x10001)
generated ./.certs/trusted-client-key.pem
Signature ok
subject=/O=machinezone/O=IXWebSocket/CN=trusted-client
Getting CA Private Key
generated ./.certs/trusted-client-crt.pem
Generating RSA private key, 2048 bit long modulus
..............+++
.......................................+++
e is 65537 (0x10001)
generated ./.certs/untrusted-ca-key.pem
generated ./.certs/untrusted-ca-crt.pem
Generating RSA private key, 2048 bit long modulus
..........+++
................................................+++
e is 65537 (0x10001)
generated ./.certs/untrusted-client-key.pem
Signature ok
subject=/O=machinezone/O=IXWebSocket/CN=untrusted-client
Getting CA Private Key
generated ./.certs/untrusted-client-crt.pem
Generating RSA private key, 2048 bit long modulus
.....................................................................................+++
...........+++
e is 65537 (0x10001)
generated ./.certs/selfsigned-client-key.pem
Signature ok
subject=/O=machinezone/O=IXWebSocket/CN=selfsigned-client
Getting Private key
generated ./.certs/selfsigned-client-crt.pem
```

Now run the snake server.

```
$ export certs=.certs
$ ws snake --tls --port 8765 --cert-file ${certs}/trusted-server-crt.pem --key-file ${certs}/trusted-server-key.pem --ca-file ${certs}/trusted-ca-crt.pem
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

As a new connection comes in, such output should be printed

```
[2019-12-19 20:27:19.724] [info] New connection
id: 0
Uri: /v2?appkey=_health
Headers:
Connection: Upgrade
Host: 127.0.0.1:8765
Sec-WebSocket-Extensions: permessage-deflate; server_max_window_bits=15; client_max_window_bits=15
Sec-WebSocket-Key: d747B0fE61Db73f7Eh47c0==
Sec-WebSocket-Protocol: json
Sec-WebSocket-Version: 13
Upgrade: websocket
User-Agent: ixwebsocket/7.5.8 macos ssl/OpenSSL OpenSSL 1.0.2q  20 Nov 2018 zlib 1.2.11
```

To connect and publish a message, do:

```
$ export certs=.certs
$ cd /path/to/ws/folder
$ ls cobraMetricsSample.json
cobraMetricsSample.json
$ ws cobra_publish --endpoint wss://127.0.0.1:8765 --appkey FC2F10139A2BAc53BB72D9db967b024f --rolename _pub --rolesecret 1c04DB8fFe76A4EeFE3E318C72d771db --channel foo --cert-file ${certs}/trusted-client-crt.pem --key-file ${certs}/trusted-client-key.pem --ca-file ${certs}/trusted-ca-crt.pem cobraMetricsSample.json
[2019-12-19 20:46:42.656] [info] Publisher connected
[2019-12-19 20:46:42.657] [info] Connection: Upgrade
[2019-12-19 20:46:42.657] [info] Sec-WebSocket-Accept: rs99IFThoBrhSg+k8G4ixH9yaq4=
[2019-12-19 20:46:42.657] [info] Sec-WebSocket-Extensions: permessage-deflate; server_max_window_bits=15; client_max_window_bits=15
[2019-12-19 20:46:42.657] [info] Server: ixwebsocket/7.5.8 macos ssl/OpenSSL OpenSSL 1.0.2q  20 Nov 2018 zlib 1.2.11
[2019-12-19 20:46:42.657] [info] Upgrade: websocket
[2019-12-19 20:46:42.658] [info] Publisher authenticated
[2019-12-19 20:46:42.658] [info] Published msg 3
[2019-12-19 20:46:42.659] [info] Published message id 3 acked
```

To use OpenSSL on macOS, compile with `make ws_openssl`. First you will have to install OpenSSL libraries, which can be done with Homebrew. Use `make ws_mbedtls` accordingly to use MbedTLS.
