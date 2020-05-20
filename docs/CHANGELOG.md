# Changelog
All changes to this project will be documented in this file.

## [9.6.4] - 2020-05-20

(compiler fix) support clang 5 and earlier (contributed by @LunarWatcher)

## [9.6.3] - 2020-05-18

(cmake) revert CMake changes to fix #203 and be able to use an external OpenSSL

## [9.6.2] - 2020-05-17

(cmake) make install cmake files optional to not conflict with vcpkg

## [9.6.1] - 2020-05-17

(windows + tls) mbedtls is the default windows tls backend + add ability to load system certificates with mbdetls on windows

## [9.6.0] - 2020-05-12

(ixbots) add options to limit how many messages per minute should be processed

## [9.5.9] - 2020-05-12

(ixbots) add new class to configure a bot to simplify passing options around

## [9.5.8] - 2020-05-08

(openssl tls) (openssl < 1.1) logic inversion - crypto locking callback are not registered properly

## [9.5.7] - 2020-05-08

(cmake) default TLS back to mbedtls on Windows Universal Platform

## [9.5.6] - 2020-05-06

(cobra bots) add a --heartbeat_timeout option to specify when the bot should terminate because no events are received

## [9.5.5] - 2020-05-06

(openssl tls) when OpenSSL is older than 1.1, register the crypto locking callback to be thread safe. Should fix lots of CI failures

## [9.5.4] - 2020-05-04

(cobra bots) do not use a queue to store messages pending processing, let the bot handle queuing

## [9.5.3] - 2020-04-29

(http client) better current request cancellation support when the HttpClient destructor is invoked (see #189)

## [9.5.2] - 2020-04-27

(cmake) fix cmake broken tls option parsing

## [9.5.1] - 2020-04-27

(http client) Set default values for most HttpRequestArgs struct members (fix #185)

## [9.5.0] - 2020-04-25

(ssl) Default to OpenSSL on Windows, since it can load the system certificates by default

## [9.4.1] - 2020-04-25

(header) Add a space between header name and header value since most http parsers expects it, although it it not required. Cf #184 and #155

## [9.4.0] - 2020-04-24

(ssl) Add support for supplying SSL CA from memory, for OpenSSL and MbedTLS backends

## [9.3.3] - 2020-04-17

(ixbots) display sent/receive message, per seconds as accumulated

## [9.3.2] - 2020-04-17

(ws) add a --logfile option to configure all logs to go to a file

## [9.3.1] - 2020-04-16

(cobra bots) add a utility class to factor out the common bots features (heartbeat) and move all bots to used it + convert cobra_subscribe to be a bot and add a unittest for it

## [9.3.0] - 2020-04-15

(websocket) add a positive number to the heartbeat message sent, incremented each time the heartbeat is sent

## [9.2.9] - 2020-04-15

(ixcobra) change cobra event callback to use a struct instead of several objects, which is more flexible/extensible

## [9.2.8] - 2020-04-15

(ixcobra) make CobraConnection_EventType an enum class (CobraEventType)

## [9.2.7] - 2020-04-14

(ixsentry) add a library method to upload a payload directly to sentry

## [9.2.6] - 2020-04-14

(ixcobra) snake server / handle invalid incoming json messages + cobra subscriber in fluentd mode insert a created_at timestamp entry

## [9.2.5] - 2020-04-13

(websocket) WebSocketMessagePtr is a unique_ptr instead of a shared_ptr

## [9.2.4] - 2020-04-13

(websocket) use persistent member variable as temp variables to encode/decode zlib messages in order to reduce transient allocations

## [9.2.3] - 2020-04-13

(ws) add a --runtime option to ws cobra_subscribe to optionally limit how much time it will run

## [9.2.2] - 2020-04-04

(third_party deps) fix #177, update bundled spdlog to 1.6.0

## [9.2.1] - 2020-04-04

(windows) when using OpenSSL, the system store is used to populate the cacert. No need to ship a cacert.pem file with your app.

## [9.2.0] - 2020-04-04

(windows) ci: windows build with TLS (mbedtls) + verify that we can be build with OpenSSL

## [9.1.9] - 2020-03-30

(cobra to statsd bot) add ability to extract a numerical value and send a timer event to statsd, with the --timer option

## [9.1.8] - 2020-03-29

(cobra to statsd bot) bot init was missing + capture socket error

## [9.1.7] - 2020-03-29

(cobra to statsd bot) add ability to extract a numerical value and send a gauge event to statsd, with the --gauge option

## [9.1.6] - 2020-03-29

(ws cobra subscriber) use a Json::StreamWriter to write to std::cout, and save one std::string allocation for each message printed

## [9.1.5] - 2020-03-29

(docker) trim down docker image (300M -> 12M) / binary built without symbol and size optimization, and source code not copied over

## [9.1.4] - 2020-03-28

(jsoncpp) update bundled copy to version 1.9.3 (at sha 3beb37ea14aec1bdce1a6d542dc464d00f4a6cec)

## [9.1.3] - 2020-03-27

(docker) alpine docker build with release with debug info, and bundle ca-certificates

## [9.1.2] - 2020-03-26

(mac ssl) rename DarwinSSL -> SecureTransport (see this too -> https://github.com/curl/curl/issues/3733)

## [9.1.1] - 2020-03-26

(websocket) fix data race accessing _socket object without mutex protection when calling wakeUpFromPoll in WebSocketTransport.cpp

## [9.1.0] - 2020-03-26

(ixcobra) add explicit event types for handshake, authentication and subscription failure, and handle those by exiting in ws_cobra_subcribe and friends

## [9.0.3] - 2020-03-24

(ws connect) display statistics about how much time it takes to stop the connection

## [9.0.2] - 2020-03-24

(socket) works with unique_ptr<Socket> instead of shared_ptr<Socket> in many places

## [9.0.1] - 2020-03-24

(socket) selectInterrupt member is an unique_ptr instead of being a shared_ptr

## [9.0.0] - 2020-03-23

(websocket) reset per-message deflate codec everytime we connect to a server/client

## [8.3.4] - 2020-03-23

(websocket) fix #167, a long standing issue with sending empty messages with per-message deflate extension (and hopefully other zlib bug)

## [8.3.3] - 2020-03-22

(cobra to statsd) port to windows and add a unittest

## [8.3.2] - 2020-03-20

(websocket+tls) fix hang in tls handshake which could lead to ANR, discovered through unittesting.

## [8.3.1] - 2020-03-20

(cobra) CobraMetricsPublisher can be configure with an ix::CobraConfig + more unittest use SSL in server + client

## [8.3.0] - 2020-03-18

(websocket) Simplify ping/pong based heartbeat implementation

## [8.2.7] - 2020-03-17

(ws) ws connect gains a new option to set the interval at which to send pings
(ws) ws echo_server gains a new option (-p) to disable responding to pings with pongs

```
IXWebSocket$ ws connect --ping_interval 2 wss://echo.websocket.org
Type Ctrl-D to exit prompt...
Connecting to url: wss://echo.websocket.org
> ws_connect: connected
[2020-03-17 23:53:02.726] [info] Uri: /
[2020-03-17 23:53:02.726] [info] Headers:
[2020-03-17 23:53:02.727] [info] Connection: Upgrade
[2020-03-17 23:53:02.727] [info] Date: Wed, 18 Mar 2020 06:45:05 GMT
[2020-03-17 23:53:02.727] [info] Sec-WebSocket-Accept: 0gtqbxW0aVL/QI/ICpLFnRaiKgA=
[2020-03-17 23:53:02.727] [info] sec-websocket-extensions:
[2020-03-17 23:53:02.727] [info] Server: Kaazing Gateway
[2020-03-17 23:53:02.727] [info] Upgrade: websocket
[2020-03-17 23:53:04.894] [info] Received pong
[2020-03-17 23:53:06.859] [info] Received pong
[2020-03-17 23:53:08.881] [info] Received pong
[2020-03-17 23:53:10.848] [info] Received pong
[2020-03-17 23:53:12.898] [info] Received pong
[2020-03-17 23:53:14.865] [info] Received pong
[2020-03-17 23:53:16.890] [info] Received pong
[2020-03-17 23:53:18.853] [info] Received pong

[2020-03-17 23:53:19.388] [info]
ws_connect: connection closed: code 1000 reason Normal closure

[2020-03-17 23:53:19.502] [info] Received 208 bytes
[2020-03-17 23:53:19.502] [info] Sent 0 bytes
```

## [8.2.6] - 2020-03-16

(cobra to sentry bot + docker) default docker file uses mbedtls + ws cobra_to_sentry pass tls options to sentryClient.

## [8.2.5] - 2020-03-13

(cobra client) ws cobra subscribe resubscribe at latest position after being disconnected

## [8.2.4] - 2020-03-13

(cobra client) can subscribe with a position

## [8.2.3] - 2020-03-13

(cobra client) pass the message position to the subscription data callback

## [8.2.2] - 2020-03-12

(openssl tls backend) Fix a hand in OpenSSL when using TLS v1.3 ... by disabling TLS v1.3

## [8.2.1] - 2020-03-11

(cobra) IXCobraConfig struct has tlsOptions and per message deflate options

## [8.2.0] - 2020-03-11

(cobra) add IXCobraConfig struct to pass cobra config around

## [8.1.9] - 2020-03-09

(ws cobra_subscribe) add a --fluentd option to wrap a message in an enveloppe so that fluentd can recognize it

## [8.1.8] - 2020-03-02

(websocket server) fix regression with disabling zlib extension on the server side. If a client does not support this extension the server will handle it fine. We still need to figure out how to disable the option.

## [8.1.7] - 2020-02-26

(websocket) traffic tracker received bytes is message size while it should be wire size

## [8.1.6] - 2020-02-26

(ws_connect) display sent/received bytes statistics on exit

## [8.1.5] - 2020-02-23

(server) give thread name to some usual worker threads / unittest is broken !!

## [8.1.4] - 2020-02-22

(websocket server) fix regression from 8.1.2, where per-deflate message compression was always disabled

## [8.1.3] - 2020-02-21

(client + server) Fix #155 / http header parser should treat the space(s) after the : delimiter as optional. Fixing this bug made us discover that websocket sub-protocols are not properly serialiazed, but start with a ,

## [8.1.2] - 2020-02-18

(WebSocketServer) add option to disable deflate compression, exposed with the -x option to ws echo_server

## [8.1.1] - 2020-02-18

(ws cobra to statsd and sentry sender) exit if no messages are received for one minute, which is a sign that something goes wrong on the server side. That should be changed to be configurable in the future

## [8.1.0] - 2020-02-13

(http client + sentry minidump upload) Multipart stream closing boundary is invalid + mark some options as mandatory in the command line tools

## [8.0.7] - 2020-02-12

(build) remove the unused subtree which was causing some way of installing to break

## [8.0.6] - 2020-01-31

(snake) add an option to disable answering pongs as response to pings, to test cobra client behavior with hanged connections

## [8.0.5] - 2020-01-31

(IXCobraConnection) set a ping timeout of 90 seconds. If no pong messages are received as responses to ping for a while, give up and close the connection

## [8.0.4] - 2020-01-31

(cobra to sentry) remove noisy logging

## [8.0.3] - 2020-01-30

(ixcobra) check if we are authenticated in publishNext before trying to publish a message

## [8.0.2] - 2020-01-28

Extract severity level when emitting messages to sentry

## [8.0.1] - 2020-01-28

Fix bug #151 - If a socket connection is interrupted, calling stop() on the IXWebSocket object blocks until the next retry

## [8.0.0] - 2020-01-26

(SocketServer) add ability to bind on an ipv6 address

## [7.9.6] - 2020-01-22

(ws) add a dnslookup sub-command, to get the ip address of a remote host

## [7.9.5] - 2020-01-14

(windows) fix #144, get rid of stubbed/un-implemented windows schannel ssl backend

## [7.9.4] - 2020-01-12

(openssl + mbedssl) fix #140, can send large files with ws send over ssl / still broken with apple ssl

## [7.9.3] - 2020-01-10

(apple ssl) model write method after the OpenSSL one for consistency

## [7.9.2] - 2020-01-06

(apple ssl) unify read and write ssl utility code

## [7.9.1] - 2020-01-06

(websocket client) better error propagation when errors are detected while sending data
(ws send) detect failures to send big files, terminate in those cases and report error

## [7.9.0] - 2020-01-04

(ws send) add option (-x) to disable per message deflate compression

## [7.8.9] - 2020-01-04

(ws send + receive) handle all message types (ping + pong + fragment) / investigate #140

## [7.8.8] - 2019-12-28

(mbedtls) fix related to private key file parsing and initialization

## [7.8.6] - 2019-12-28

(ws cobra to sentry/statsd) fix for handling null events properly for empty queues + use queue to send data to statsd

## [7.8.5] - 2019-12-28

(ws cobra to sentry) handle null events for empty queues

## [7.8.4] - 2019-12-27

(ws cobra to sentry) game is picked in a fair manner, so that all games get the same share of sent events

## [7.8.3] - 2019-12-27

(ws cobra to sentry) refactor queue related code into a class

## [7.8.2] - 2019-12-25

(ws cobra to sentry) bound the queue size used to hold up cobra messages before they are sent to sentry. Default queue size is a 100 messages. Without such limit the program runs out of memory when a subscriber receive a lot of messages that cannot make it to sentry

## [7.8.1] - 2019-12-25

(ws client) use correct compilation defines so that spdlog is not used as a header only library (reduce binary size and increase compilation speed)

## [7.8.0] - 2019-12-24

(ws client) all commands use spdlog instead of std::cerr or std::cout for logging

## [7.6.5] - 2019-12-24

(cobra client) send a websocket ping every 30s to keep the connection opened

## [7.6.4] - 2019-12-22

(client) error handling, quote url in error case when failing to parse one
(ws) ws_cobra_publish: register callbacks before connecting
(doc) mention mbedtls in supported ssl server backend

## [7.6.3] - 2019-12-20

(tls) add a simple description of the TLS configuration routine for debugging

## [7.6.2] - 2019-12-20

(mbedtls) correct support for using own certificate and private key

## [7.6.1] - 2019-12-20

(ws commands) in websocket proxy, disable automatic reconnections + in Dockerfile, use alpine 3.11

## [7.6.0] - 2019-12-19

(cobra) Add TLS options to all cobra commands and classes. Add example to the doc.

## [7.5.8] - 2019-12-18

(cobra-to-sentry) capture application version from device field

## [7.5.7] - 2019-12-18

(tls) Experimental TLS server support with mbedtls (windows) + process cert tlsoption (client + server)

## [7.5.6] - 2019-12-18

(tls servers) Make it clear that apple ssl and mbedtls backends do not support SSL in server mode

## [7.5.5] - 2019-12-17

(tls options client) TLSOptions struct _validated member should be initialized to false

## [7.5.4] - 2019-12-16

(websocket client) improve the error message when connecting to a non websocket server

Before:

```
Connection error: Got bad status connecting to example.com:443, status: 200, HTTP Status line: HTTP/1.1 200 OK
```

After:

```
Connection error: Expecting status 101 (Switching Protocol), got 200 status connecting to example.com:443, HTTP Status line: HTTP/1.1 200 OK
```

## [7.5.3] - 2019-12-12

(server) attempt at fixing #131 by using blocking writes in server mode

## [7.5.2] - 2019-12-11

(ws) cobra to sentry - created events with sentry tags based on tags present in the cobra messages

## [7.5.1] - 2019-12-06

(mac) convert SSL errors to utf8

## [7.5.0] - 2019-12-05

- (ws) cobra to sentry. Handle Error 429 Too Many Requests and politely wait before sending more data to sentry.

In the example below sentry we are sending data too fast, sentry asks us to slow down which we do. Notice how the sent count stop increasing, while we are waiting for 41 seconds.

```
[2019-12-05 15:50:33.759] [info] messages received 2449 sent 3
[2019-12-05 15:50:34.759] [info] messages received 5533 sent 7
[2019-12-05 15:50:35.759] [info] messages received 8612 sent 11
[2019-12-05 15:50:36.759] [info] messages received 11562 sent 15
[2019-12-05 15:50:37.759] [info] messages received 14410 sent 19
[2019-12-05 15:50:38.759] [info] messages received 17236 sent 23
[2019-12-05 15:50:39.282] [error] Error sending data to sentry: 429
[2019-12-05 15:50:39.282] [error] Body: {"exception":[{"stacktrace":{"frames":[{"filename":"WorldScene.lua","function":"WorldScene.lua:1935","lineno":1958},{"filename":"WorldScene.lua","function":"onUpdate_WorldCam","lineno":1921},{"filename":"WorldMapTile.lua","function":"__index","lineno":239}]},"value":"noisytypes: Attempt to call nil(nil,2224139838)!"}],"platform":"python","sdk":{"name":"ws","version":"1.0.0"},"tags":[["game","niso"],["userid","107638363"],["environment","live"]],"timestamp":"2019-12-05T23:50:39Z"}

[2019-12-05 15:50:39.282] [error] Response: {"error_name":"rate_limit","error":"Creation of this event was denied due to rate limiting"}
[2019-12-05 15:50:39.282] [warning] Error 429 - Too Many Requests. ws will sleep and retry after 41 seconds
[2019-12-05 15:50:39.760] [info] messages received 18839 sent 25
[2019-12-05 15:50:40.760] [info] messages received 18839 sent 25
[2019-12-05 15:50:41.760] [info] messages received 18839 sent 25
[2019-12-05 15:50:42.761] [info] messages received 18839 sent 25
[2019-12-05 15:50:43.762] [info] messages received 18839 sent 25
[2019-12-05 15:50:44.763] [info] messages received 18839 sent 25
[2019-12-05 15:50:45.768] [info] messages received 18839 sent 25
```

## [7.4.5] - 2019-12-03

- (ws) #125 / fix build problem when jsoncpp is not installed locally

## [7.4.4] - 2019-12-03

- (ws) #125 / cmake detects an already installed jsoncpp and will try to use this one if present

## [7.4.3] - 2019-12-03

- (http client) use std::unordered_map instead of std::map for HttpParameters and HttpFormDataParameters class aliases

## [7.4.2] - 2019-12-02

- (client) internal IXDNSLookup class requires a valid cancellation request function callback to be passed in

## [7.4.1] - 2019-12-02

- (client) fix an overflow in the exponential back off code

## [7.4.0] - 2019-11-25

- (http client) Add support for multipart HTTP POST upload
- (ixsentry) Add support for uploading a minidump to sentry

## [7.3.5] - 2019-11-20

- On Darwin SSL, add ability to skip peer verification.

## [7.3.4] - 2019-11-20

- 32-bits compile fix, courtesy of @fcojavmc

## [7.3.1] - 2019-11-16

- ws proxy_server / remote server close not forwarded to the client

## [7.3.0] - 2019-11-15

- New ws command: `ws proxy_server`.

## [7.2.2] - 2019-11-01

- Tag a release + minor reformating.

## [7.2.1] - 2019-10-26

- Add unittest to IXSentryClient to lua backtrace parsing code

## [7.2.0] - 2019-10-24

- Add cobra_metrics_to_redis sub-command to create streams for each cobra metric event being received.

## [7.1.0] - 2019-10-13

- Add client support for websocket subprotocol. Look for the new addSubProtocol method for details.

## [7.0.0] - 2019-10-01

- TLS support in server code, only implemented for the OpenSSL SSL backend for now.

## [6.3.4] - 2019-09-30

- all ws subcommands propagate tls options to servers (unimplemented) or ws or http client (implemented) (contributed by Matt DeBoer)

## [6.3.3] - 2019-09-30

- ws has a --version option

## [6.3.2] - 2019-09-29

- (http + websocket clients) can specify cacert and some other tls options (not implemented on all backend). This makes it so that server certs can finally be validated on windows.

## [6.3.1] - 2019-09-29

- Add ability to use OpenSSL on apple platforms.

## [6.3.0] - 2019-09-28

- ixcobra / fix crash in CobraConnection::publishNext when the queue is empty + handle CobraConnection_PublishMode_Batch in CobraMetricsThreadedPublisher

## [6.2.9] - 2019-09-27

- mbedtls fixes / the unittest now pass on macOS, and hopefully will on Windows/AppVeyor as well.

## [6.2.8] - 2019-09-26

- Http server: add options to ws https to redirect all requests to a given url. POST requests will get a 200 and an empty response.

```
ws httpd -L --redirect_url https://www.google.com
```

## [6.2.7] - 2019-09-25

- Stop having ws send subcommand send a binary message in text mode, which would cause error in `make ws_test` shell script test.

## [6.2.6] - 2019-09-24

- Fix 2 race conditions detected with TSan, one in CobraMetricsPublisher::push and another one in WebSocketTransport::sendData (that one was bad).

## [6.2.5] - 2019-09-23

- Add simple Redis Server which is only capable of doing publish / subscribe. New ws redis_server sub-command to use it. The server is used in the unittest, so that we can run on CI in environment where redis isn not available like github actions env.

## [6.2.4] - 2019-09-22

- Add options to configure TLS ; contributed by Matt DeBoer. Only implemented for OpenSSL TLS backend for now.

## [6.2.3] - 2019-09-21

- Fix crash in the Linux unittest in the HTTP client code, in Socket::readBytes
- Cobra Metrics Publisher code returns the message id of the message that got published, to be used to validated that it got sent properly when receiving an ack.

## [6.2.2] - 2019-09-19

- In DNS lookup code, make sure the weak pointer we use lives through the expected scope (if branch)

## [6.2.1] - 2019-09-17

- On error while doing a client handshake, additionally display port number next to the host name

## [6.2.0] - 2019-09-09

- websocket and http server: server does not close the bound client socket in many cases
- improve some websocket error messages
- add a utility function with unittest to parse status line and stop using scanf which triggers warnings on Windows
- update ws CLI11 (our command line argument parsing library) to the latest, which fix a compiler bug about optional

## [6.1.0] - 2019-09-08

- move poll wrapper on top of select (only used on Windows) to the ix namespace

## [6.0.1] - 2019-09-05

- add cobra metrics publisher + server unittest
- add cobra client + server unittest
- ws snake (cobra simple server) add basic support for unsubscription + subscribe send the proper subscription data + redis client subscription can be cancelled
- IXCobraConnection / pdu handlers can crash if they receive json data which is not an object

## [6.0.0] - 2019-09-04

- all client autobahn test should pass !
- zlib/deflate has a bug with windowsbits == 8, so we silently upgrade it to 9/ (fix autobahn test 13.X which uses 8 for the windows size)

## [5.2.0] - 2019-09-04

- Fragmentation: for sent messages which are compressed, the continuation fragments should not have the rsv1 bit set (fix all autobahn tests for zlib compression 12.X)
- Websocket Server / do a case insensitive string search when looking for an Upgrade header whose value is websocket. (some client use WebSocket with some upper-case characters)

## [5.1.9] - 2019-09-03

- ws autobahn / report progress with spdlog::info to get timing info
- ws autobahn / use condition variables for stopping test case + add more logging on errors

## [5.1.8] - 2019-09-03

- Per message deflate/compression: handle fragmented messages (fix autobahn test: 12.1.X and probably others)

## [5.1.7] - 2019-09-03

- Receiving invalid UTF-8 TEXT message should fail and close the connection (fix remaining autobahn test: 6.X UTF-8 Handling)

## [5.1.6] - 2019-09-03

- Sending invalid UTF-8 TEXT message should fail and close the connection (fix remaining autobahn test: 6.X UTF-8 Handling)
- Fix failing unittest which was sending binary data in text mode with WebSocket::send to call properly call WebSocket::sendBinary instead.
- Validate that the reason is proper utf-8. (fix autobahn test 7.5.1)
- Validate close codes. Autobahn 7.9.*

## [5.1.5] - 2019-09-03

Framentation: data and continuation blocks received out of order (fix autobahn test: 5.9 through 5.20 Fragmentation)

## [5.1.4] - 2019-09-03

Sending invalid UTF-8 TEXT message should fail and close the connection (fix **tons** of autobahn test: 6.X UTF-8 Handling)

## [5.1.3] - 2019-09-03

Message type (TEXT or BINARY) is invalid for received fragmented messages (fix autobahn test: 5.3 through 5.8 Fragmentation)

## [5.1.2] - 2019-09-02

Ping and Pong messages cannot be fragmented (fix autobahn test: 5.1 and 5.2 Fragmentation)

## [5.1.1] - 2019-09-01

Close connections when reserved bits are used (fix autobahn test: 3.X Reserved Bits)

## [5.1.0] - 2019-08-31

- ws autobahn / Add code to test websocket client compliance with the autobahn test-suite
- add utf-8 validation code, not hooked up properly yet
- Ping received with a payload too large (> 125 bytes) trigger a connection closure
- cobra / add tracking about published messages
- cobra / publish returns a message id, that can be used when
- cobra / new message type in the message received handler when publish/ok is received (can be used to implement an ack system).

## [5.0.9] - 2019-08-30

- User-Agent header is set when not specified.
- New option to cap the max wait between reconnection attempts. Still default to 10s. (setMaxWaitBetweenReconnectionRetries).

```
ws connect --max_wait 5000 ws://example.com # will only wait 5 seconds max between reconnection attempts
```

## [5.0.7] - 2019-08-23
- WebSocket: add new option to pass in extra HTTP headers when connecting.
- `ws connect` add new option (-H, works like [curl](https://stackoverflow.com/questions/356705/how-to-send-a-header-using-a-http-request-through-a-curl-call)) to pass in extra HTTP headers when connecting

If you run against `ws echo_server` you will see the headers being received printed in the terminal.
```
ws connect -H "foo: bar" -H "baz: buz" ws://127.0.0.1:8008
```

- CobraConnection: sets a unique id field for all messages sent to [cobra](https://github.com/machinezone/cobra).
- CobraConnection: sets a counter as a field for each event published.

## [5.0.6] - 2019-08-22
- Windows: silly compile error (poll should be in the global namespace)

## [5.0.5] - 2019-08-22
- Windows: use select instead of WSAPoll, through a poll wrapper

## [5.0.4] - 2019-08-20
- Windows build fixes (there was a problem with the use of ::poll that has a different name on Windows (WSAPoll))

## [5.0.3] - 2019-08-14
- CobraMetricThreadedPublisher _enable flag is an atomic, and CobraMetricsPublisher is enabled by default

## [5.0.2] - 2019-08-01
- ws cobra_subscribe has a new -q (quiet) option
- ws cobra_subscribe knows to and display msg stats (count and # of messages received per second)
- ws cobra_subscribe, cobra_to_statsd and cobra_to_sentry commands have a new option, --filter to restrict the events they want to receive

## [5.0.1] - 2019-07-25
- ws connect command has a new option to send in binary mode (still default to text)
- ws connect command has readline history thanks to libnoise-cpp. Now ws connect one can use using arrows to lookup previous sent messages and edit them

## [5.0.0] - 2019-06-23
### Changed
- New HTTP server / still very early. ws gained a new command, httpd can run a simple webserver serving local files.
- IXDNSLookup. Uses weak pointer + smart_ptr + shared_from_this instead of static sets + mutex to handle object going away before dns lookup has resolved
- cobra_to_sentry / backtraces are reversed and line number is not extracted correctly
- mbedtls and zlib are searched with find_package, and we use the vendored version if nothing is found
- travis CI uses g++ on Linux

## [4.0.0] - 2019-06-09
### Changed
- WebSocket::send() sends message in TEXT mode by default
- WebSocketMessage sets a new binary field, which tells whether the received incoming message is binary or text
- WebSocket::send takes a third arg, binary which default to true (can be text too)
- WebSocket callback only take one object, a const ix::WebSocketMessagePtr& msg
- Add explicit WebSocket::sendBinary method
- New headers + WebSocketMessage class to hold message data, still not used across the board
- Add test/compatibility folder with small servers and clients written in different languages and different libraries to test compatibility.
- ws echo_server has a -g option to print a greeting message on connect
- IXSocketMbedTLS: better error handling in close and connect

## [3.1.2] - 2019-06-06
### Added
- ws connect has a -x option to disable per message deflate
- Add WebSocket::disablePerMessageDeflate() option.

## [3.0.0] - 2019-06-xx
### Changed
- TLS, aka SSL works on Windows (websocket and http clients)
- ws command line tool build on Windows
- Async API for HttpClient
- HttpClient API changed to use shared_ptr for response and request
