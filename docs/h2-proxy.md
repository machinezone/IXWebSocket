# WebSockets behind an HTTP/2 reverse proxy

The integration test exercises this flow with the current IXWebSocket source:

```text
RFC 8441 client -- TLS + HTTP/2 --> nghttpx -- HTTP/1.1 Upgrade --> IXWebSocket
```

The frontend client negotiates `h2` through ALPN, waits for
`SETTINGS_ENABLE_CONNECT_PROTOCOL`, and sends Extended CONNECT with
`:protocol: websocket`. It requires an HTTP/2 200 response. It does not fall back
to HTTP/1.1 or send `Sec-WebSocket-Key`; the proxy must synthesize the HTTP/1.1
handshake for the backend. The backend uses the existing `WebSocketServer` API
and its normal HTTP/1.1 handshake validation.

## Run locally

With Docker running, from the repository root:

```sh
make -f makefile.dev test_h2_proxy
```

The target builds the current source, installs the proxy and pinned Python test
clients inside an image, and runs all services in one isolated container. It
publishes no ports and needs no host Python packages or proxy installation.
Docker supplies the test environment; it is not a library dependency.

Progress prints one PASS line per group. Each run retains `report.json`, backend
logs, and proxy access/error logs under `build/h2-proxy/runs/run-*`. Image build
output goes to `build/h2-proxy/build.log`. Backend/proxy startup, network operations,
and process shutdown have bounded waits. The fixture asks the backend to stop
normally and checks that it joins its workers and exits successfully.

Coverage includes:

- Native IXWebSocket H2 client with compression on/off and both close directions.
- TLS certificate validation, ALPN, Extended CONNECT, and HTTP/2 success status.
- Forwarded Origin/custom headers, paths, and query strings.
- Empty, Unicode and binary messages, fragmentation, and ping/pong.
- Per-message deflate enabled and disabled; 256 KiB incompressible messages that
  exceed the HTTP/2 flow-control window.
- Three simultaneous WebSocket streams on one HTTP/2 connection.
- Closing or resetting one stream while another continues exchanging messages.
- Client/server-initiated close codes and reasons, followed by a new WebSocket
  stream on the same connection.

After the WebSocket Close exchange, nghttpx can retire a stream with
`RST_STREAM(NO_ERROR)` rather than only END_STREAM. The client accepts that only
when both WebSocket Close frames have been exchanged. Premature resets and other
reset codes fail the test.

The native C++ client runs through the proxy in four compression/close modes;
it also runs against an independent Python TLS/H2 peer for simultaneous large
transfers, blocked-send cancellation and ALPN rejection. The Python
`h2`/`wsproto` client then checks the proxy/backend flow,
including multiplexing. The C++ client currently uses one stream per connection.
This is protocol integration coverage, not browser automation, an Autobahn run,
or a performance benchmark. It runs separately from the regular unit suite.

## Native run and Envoy reproduction

Docker is optional if CMake, a C++11 compiler, zlib development files, Python 3.10+,
OpenSSL's command-line tool, and nghttpx are installed. On macOS, the nghttp2
Homebrew formula supplies nghttpx:

```sh
brew install nghttp2
make -f makefile.dev test_h2_proxy_native
```

The runner also includes an Envoy configuration:

```sh
make -f makefile.dev test_h2_proxy_native H2_PROXY_ARGS='--proxy envoy'
```

**Envoy 1.31.0 fails this strict flow:** it translates the Extended CONNECT request
to HTTP/1.1 but omits `Sec-WebSocket-Key`. IXWebSocket correctly returns HTTP 400
with `Missing Sec-WebSocket-Key value`. This reproduces
[Envoy issue #8547](https://github.com/envoyproxy/envoy/issues/8547); the upstream
[fix proposal #46959](https://github.com/envoyproxy/envoy/pull/46959) was still
unmerged when this test was added. The Envoy mode deliberately reports failure
on affected versions. Neither the test client nor IXWebSocket works around the
proxy defect by supplying a legacy key or weakening backend validation.

Use the flags `--envoy /path/to/envoy`, `--nghttpx /path/to/nghttpx`, or
`--openssl /path/to/openssl` through `H2_PROXY_ARGS` to select local binaries.

## CI

The `h2-proxy` workflow runs on master pushes and manual dispatch. Add the
`h2-proxy` label to opt a PR into the check. It uses the Docker target and uploads
reports and logs even on failure. It does not add time to the unit-test executable.
