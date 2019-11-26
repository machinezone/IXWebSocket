# Changelog
All notable changes to this project will be documented in this file.

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
