## Benchmarking WebSocket reception

Build optimized code before comparing callback performance:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DUSE_WS=ON -DUSE_TEST=ON
cmake --build build --parallel
```

The defaults are a quick 100,000 messages and three measured runs per mode. Increase
the counts explicitly for a more stable performance comparison.

The new `ws bench` command follows [autoroute](https://github.com/bsergean/autoroute)'s
receive-N-text-messages protocol and works with `ws push_server`. It preserves the
existing `ws autoroute` command. No Docker installation or additional package is
needed; JSON reports use the C++ standard library.

```sh
# Keep this running in another terminal.
./build/ws/ws push_server --keep-running -q -x --send_msg 'hello world'

./build/ws/ws bench ws://127.0.0.1:8008 --messages 1000000 --runs 7 --warmup 1 --callback both --json > bench.json
```

Both modes use the same receive/counting code. Trials alternate callback order,
exclude TCP/TLS connection setup, and do no per-message logging or locking. An
approximate progress count is published once per 1024 messages and displayed once
per second on stderr. Warmup results are omitted. Reports retain individual trial
rates plus median/min/max, payload-byte throughput, and the difference between the
two medians. Inspect the spread rather than treating every percentage as a win.

This measures the complete server/network/client pipeline, including UTF-8 checks,
frame parsing, buffering, and scheduling. A push server that sends one frame per
system call may hide client-side improvements. Different payloads, compression,
TLS, operating systems, and server implementations can produce different results.
The result is throughput, not round-trip latency.

### Receiver-bound push fixture

To reduce the sender's per-frame system-call overhead, use the optional standard-library
Python fixture. It sends the same autoroute text messages, batched 8192 frames per
write by default. It is for local benchmarks and supports neither TLS nor compression:

```sh
python3 tools/bench_push.py --port 8008
./build/ws/ws bench ws://127.0.0.1:8008 --messages 1000000 --runs 7 --warmup 1 --json
```

### Dispatch and allocation checks

The normal unit suite checks that legacy callbacks allocate one event object and
reference callbacks avoid that allocation with RTTI enabled. It does not enforce
any speed threshold. No-RTTI builds keep working through the allocation fallback.

A hidden, opt-in Release test measures the actual event dispatcher without sockets:

```sh
./build/test/IXWebSocketCallbackTest '[.callback_dispatch]'
```

It alternates nine trials per mode, after warmup, and prints JSON lines with ns/event.
The tiny callback consumes payload size and wire size. This test executable also
contains allocation instrumentation, so these timings isolate dispatch costs rather
than predict whole-application gains. It is excluded from default test runs.

Existing callback types, the legacy setter (including `nullptr`), and callback member
storage remain intact. Reference delivery is opt-in. The payload is still borrowed;
see [reference callbacks](usage.md#reference-message-callbacks).


### Local callback comparison

One Apple M4 Max / macOS 26.4 / Apple Clang 21 Release run, with RTTI enabled,
uncompressed 11-byte messages, one million messages per connection, one warmup and
nine measured trials per variant. Trial order rotates across all three variants.
The baseline uses the same benchmark driver compiled against unchanged master
`fc19f0e5`; its reference option is not used.

| Sender / measurement | Unchanged master, legacy | Candidate, legacy | Candidate, reference |
| --- | ---: | ---: | ---: |
| `ws push_server`, median messages/s | 671,738 | 673,272 | 664,051 |
| Batched fixture, median messages/s | 5,639,955 | 5,589,038 | 5,913,066 |
| Dispatch only, median ns/event | — | 23.39 | 12.90 |

The ordinary push-server test shows **no throughput improvement**: reference delivery
was 1.4% below candidate legacy by median, with overlapping trial ranges. With the
batched sender, reference delivery was 5.8% above candidate legacy and 4.8% above
unchanged master. Candidate legacy was 0.9% below master in that receiver-bound
setup; its range overlapped the baseline. Dispatch-only event time fell about 45%,
but that is not an end-to-end speedup. The allocation reduction is reliable; the
throughput benefit depends on which part of the pipeline limits the application.

Raw trials: [push server](benchmarks/callback-autoroute.json),
[batched sender](benchmarks/callback-batched.json),
[dispatch](benchmarks/callback-dispatch.json). These are local measurements, not
portable guarantees. An old-header consumer also linked and echoed successfully
against the candidate shared library on this Mac; object sizes matched (WebSocket
1792 bytes, WebSocketServer 888 bytes). Other toolchains still require their normal
compatibility checks.

## Historical measurement (2015)

We will run a client and a server on the same machine, connecting to localhost. This bench is run on a MacBook Pro from 2015. We can receive over 200,000 (small) messages per second, another way to put it is that it takes 5 micro-second to receive and process one message. This is an indication about the minimal latency to receive messages.

### Receiving messages

By using the push_server ws sub-command, the server will send the same message in a loop to any connected client.

```
ws push_server -q --send_msg 'yo'
```

By using the echo_client ws sub-command, with the -m (mute or no_send), we will display statistics on how many messages we can receive per second.

```
$ ws echo_client -m ws://localhost:8008
[2020-08-02 12:31:17.284] [info] ws_echo_client: connected
[2020-08-02 12:31:17.284] [info] Uri: /
[2020-08-02 12:31:17.284] [info] Headers:
[2020-08-02 12:31:17.284] [info] Connection: Upgrade
[2020-08-02 12:31:17.284] [info] Sec-WebSocket-Accept: byy/pMK2d0PtRwExaaiOnXJTQHo=
[2020-08-02 12:31:17.284] [info] Server: ixwebsocket/10.1.4 macos ssl/SecureTransport zlib 1.2.11
[2020-08-02 12:31:17.284] [info] Upgrade: websocket
[2020-08-02 12:31:17.663] [info] messages received: 0 per second 2595307 total
[2020-08-02 12:31:18.668] [info] messages received: 79679 per second 2674986 total
[2020-08-02 12:31:19.668] [info] messages received: 207438 per second 2882424 total
[2020-08-02 12:31:20.673] [info] messages received: 209207 per second 3091631 total
[2020-08-02 12:31:21.676] [info] messages received: 216056 per second 3307687 total
[2020-08-02 12:31:22.680] [info] messages received: 214927 per second 3522614 total
[2020-08-02 12:31:23.684] [info] messages received: 216960 per second 3739574 total
[2020-08-02 12:31:24.688] [info] messages received: 215232 per second 3954806 total
[2020-08-02 12:31:25.691] [info] messages received: 212300 per second 4167106 total
[2020-08-02 12:31:26.694] [info] messages received: 212501 per second 4379607 total
[2020-08-02 12:31:27.699] [info] messages received: 212330 per second 4591937 total
[2020-08-02 12:31:28.702] [info] messages received: 216511 per second 4808448 total
```
