# Changelog
All changes to this project will be documented in this file.

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
