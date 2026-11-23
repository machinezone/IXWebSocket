# Experimental WebSocket client over HTTP/2

IXWebSocket has an opt-in, self-contained RFC 8441 client. It implements its own
HTTP/2 framing and HPACK codec and adds no HTTP/2 library dependency.
HTTP/1.1 remains the default.

```cpp
ix::WebSocket socket;
socket.setUrl("wss://example.com/echo");
socket.setHttp2(true);
socket.setOnMessageCallback([](const ix::WebSocketMessagePtr& message)
{
    // Existing Open, Message, Ping/Pong, Error and Close events are unchanged.
});
socket.start();
```

The command-line client also supports explicit H2 mode:

```sh
ws connect wss://example.com/echo --http2
```

Configure before connecting, as with other WebSocket settings. HTTP/2 mode
requires `wss://`, TLS 1.2 or newer, ALPN `h2`, and a peer advertising
`SETTINGS_ENABLE_CONNECT_PROTOCOL`. It uses Extended CONNECT and accepts a
successful 2xx response. If negotiation fails, the connection fails explicitly;
it does not silently switch to HTTP/1.1. The normal automatic-reconnection
setting still applies.

This first implementation uses **one WebSocket stream per TCP/TLS connection**.
It does not pool connections or multiplex several WebSocket objects on one
connection. The HTTP client and the WebSocket/HTTP servers remain HTTP/1.1.
`connect(HttpClient&, timeout)` cannot transfer an HTTP/1.1 connection into this
mode. Cleartext HTTP/2 (`h2c`) is not implemented.

The TLS layer negotiates ALPN using the selected existing backend: OpenSSL/
LibreSSL, mbedTLS when built with ALPN support, Network.framework, or
SecureTransport. Existing backend certificate/trust limitations still apply.
Normal certificate and hostname verification remain enabled by default.

Existing WebSocket framing, masking, fragmentation, compression, message limits,
callbacks, ping/pong and close handling are reused through an H2 stream adapter.
The adapter handles SETTINGS, HEADERS/CONTINUATION, flow control, PING,
WINDOW_UPDATE, GOAWAY, RST_STREAM and stream termination. Server push is disabled.
HPACK responses support the static/dynamic tables and Huffman coding. Outgoing
fields are literal and never indexed, including credentials. Compressed and
decoded header blocks are bounded at 64 KiB, the dynamic table at 4 KiB, incoming
DATA at the advertised receive window, and pending control frames at 64.

Extra header names are encoded lowercase; Host is mapped to `:authority`.
HTTP/1.1-only headers such as Connection, Upgrade and Sec-WebSocket-Key are
rejected. Default User-Agent/Origin suppression and redirect credential handling
retain their existing APIs. The HTTP status and response headers remain available
on handshake failures.

This is an experimental transport path with a deliberately limited connection
model. Rebuild applications against the updated headers/library.

## Testing

```sh
make -f makefile.dev test_h2_proxy
```

The Docker integration test now also runs the native C++ client through nghttpx
with compression on/off and client/server-initiated closes. It checks Unicode,
empty/binary payloads, 256 KiB messages, ping/pong and close codes/reasons. The
independent Python H2 client continues to exercise the proxy and backend.
Reports and logs remain under `build/h2-proxy/runs/`.

Fast `IXHttp2Test` unit tests cover RFC 7541 vectors, the complete Huffman byte
alphabet, dynamic tables, malformed/truncated input, fragmented headers/reads,
partial writes, flow-control limits, cancellation and remote half closes.
They use an in-memory peer and have no deliberate sleeps or DNS dependencies.

For another TLS backend, configure the fixture separately, for example:

```sh
cmake -S test/h2_proxy -B build/h2-network \
  -DH2_PROXY_NATIVE_CLIENT=ON -DUSE_NETWORK_FRAMEWORK=ON
cmake --build build/h2-network --parallel
```

With the test requirements installed in a venv, `test/h2_proxy/native_peer.py`
runs the native executable against an independent Python TLS/H2 peer:

```sh
python3 test/h2_proxy/native_peer.py \
  build/h2-network/h2_proxy_client server.pem server.key ca.pem
```

The certificates should be a server certificate/key and its trusted CA, with
`127.0.0.1` in the server certificate's subject alternative names. This check
also exercises simultaneous large messages in both directions, cancellation while
flow control blocks sending, and compression on/off without requiring a proxy.
Add `--reject-alpn` to verify that a peer without `h2` is rejected before any
HTTP bytes are sent.
See [the proxy test documentation](h2-proxy.md) for native proxy setup and the
known Envoy downgrade-handshake limitation.
