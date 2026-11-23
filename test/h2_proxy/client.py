"""A real RFC 8441 client: TLS/ALPN, H2 Extended CONNECT, then WebSocket frames."""
from collections import deque
import socket
import ssl
import time

from h2.config import H2Configuration
from h2.connection import H2Connection
from h2.events import (ConnectionTerminated, DataReceived, ResponseReceived,
                       StreamEnded, StreamReset)
from h2.settings import SettingCodes
from wsproto import Connection, ConnectionType
from wsproto.events import BytesMessage, CloseConnection, TextMessage
from wsproto.extensions import PerMessageDeflate


class Client:
    def __init__(self, port, certificate, timeout=5):
        context = ssl.create_default_context(cafile=str(certificate))
        context.set_alpn_protocols(["h2"])
        self.socket = context.wrap_socket(
            socket.create_connection(("127.0.0.1", port), timeout=timeout),
            server_hostname="localhost")
        if self.socket.selected_alpn_protocol() != "h2":
            self.socket.close()
            raise RuntimeError("Proxy did not negotiate h2 through ALPN")
        self.timeout = timeout
        self.h2 = H2Connection(config=H2Configuration(header_encoding="utf-8"))
        self.streams = {}
        self.flow_control_waits = 0
        self.h2.initiate_connection()
        self.flush()
        self.wait(lambda: self.h2.remote_settings[SettingCodes.ENABLE_CONNECT_PROTOCOL] == 1)

    def flush(self):
        data = self.h2.data_to_send()
        if data:
            self.socket.sendall(data)

    def pump(self, deadline):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("Timed out waiting for HTTP/2 or WebSocket progress")
        self.socket.settimeout(remaining)
        data = self.socket.recv(65536)
        if not data:
            raise RuntimeError("HTTP/2 connection closed unexpectedly")
        for event in self.h2.receive_data(data):
            if isinstance(event, ConnectionTerminated):
                raise RuntimeError("Unexpected HTTP/2 GOAWAY: " + str(event))
            if isinstance(event, ResponseReceived):
                state = self.streams[event.stream_id]
                headers = dict(event.headers)
                if headers.get(":status") != "200":
                    raise RuntimeError("Extended CONNECT failed: " + repr(headers))
                extensions = []
                if state["compression"]:
                    value = headers.get("sec-websocket-extensions", "")
                    if not value.startswith("permessage-deflate"):
                        raise RuntimeError("Compression was not negotiated: " + repr(headers))
                    state["extension"].finalize(value)
                    extensions.append(state["extension"])
                elif "sec-websocket-extensions" in headers:
                    raise RuntimeError("Unexpected compression negotiation")
                state["ws"] = Connection(ConnectionType.CLIENT, extensions=extensions)
            elif isinstance(event, DataReceived):
                state = self.streams[event.stream_id]
                if state["ws"] is None:
                    raise RuntimeError("Data before successful Extended CONNECT")
                # Empty DATA (including END_STREAM) carries no WebSocket bytes.
                if event.data:
                    state["ws"].receive_data(event.data)
                    frames = list(state["ws"].events())
                    if any(isinstance(frame, CloseConnection) for frame in frames):
                        state["close_received"] = True
                    state["events"].extend(frames)
                self.h2.acknowledge_received_data(event.flow_controlled_length, event.stream_id)
            elif isinstance(event, StreamEnded):
                self.streams[event.stream_id]["ended"] = True
            elif isinstance(event, StreamReset):
                state = self.streams[event.stream_id]
                # nghttpx may retire a completed WebSocket with NO_ERROR after
                # forwarding both Close frames. Never accept a premature reset.
                if event.error_code or not (state["close_sent"] and state["close_received"]):
                    raise RuntimeError("Unexpected HTTP/2 RST_STREAM: " + str(event))
                state["ended"] = state["reset"] = True
        self.flush()

    def wait(self, predicate):
        deadline = time.monotonic() + self.timeout
        while not predicate():
            self.pump(deadline)

    def open(self, path, compression=False):
        stream = self.h2.get_next_available_stream_id()
        extension = PerMessageDeflate(client_no_context_takeover=True,
                                      server_no_context_takeover=True)
        self.streams[stream] = dict(ws=None, events=deque(), ended=False,
                                    compression=compression, extension=extension,
                                    close_sent=False, close_received=False, reset=False)
        headers = [(":method", "CONNECT"), (":protocol", "websocket"),
                   (":scheme", "https"), (":authority", "localhost"), (":path", path),
                   ("sec-websocket-version", "13"), ("origin", "https://client.example"),
                   ("x-proxy-test", "h2-integration")]
        # RFC 8441 replaces the HTTP/1.1 Sec-WebSocket-Key/Accept exchange.
        if compression:
            headers.append(("sec-websocket-extensions", "permessage-deflate; " + extension.offer()))
        self.h2.send_headers(stream, headers, end_stream=False)
        self.flush()
        self.wait(lambda: self.streams[stream]["ws"] is not None)
        return stream

    def send(self, stream, event):
        if isinstance(event, CloseConnection):
            self.streams[stream]["close_sent"] = True
        data = self.streams[stream]["ws"].send(event)
        offset = 0
        deadline = time.monotonic() + self.timeout
        while offset < len(data):
            count = min(self.h2.local_flow_control_window(stream),
                        self.h2.max_outbound_frame_size, len(data) - offset)
            if not count:
                self.flow_control_waits += 1
                self.pump(deadline)
                continue
            self.h2.send_data(stream, data[offset:offset + count])
            offset += count
            self.flush()

    def event(self, stream):
        state = self.streams[stream]
        self.wait(lambda: bool(state["events"]) or state["ended"])
        if not state["events"]:
            raise RuntimeError("Stream ended without the expected WebSocket event")
        return state["events"].popleft()

    def message(self, stream, expected):
        cls = TextMessage if isinstance(expected, str) else BytesMessage
        value = "" if isinstance(expected, str) else b""
        while True:
            event = self.event(stream)
            if not isinstance(event, cls):
                raise RuntimeError("Unexpected WebSocket event: " + repr(event))
            value += event.data
            if event.message_finished:
                if value != expected:
                    raise RuntimeError("Echoed message differs from the original")
                return

    def close_stream(self, stream, server_initiated=False):
        if server_initiated:
            self.send(stream, TextMessage(data="close-from-server"))
        else:
            self.send(stream, CloseConnection(code=1000, reason="client close test"))
        event = self.event(stream)
        code = 4000 if server_initiated else 1000
        reason = "server close test" if server_initiated else "client close test"
        if not isinstance(event, CloseConnection) or event.code != code or event.reason != reason:
            raise RuntimeError("Unexpected close response: " + repr(event))
        if server_initiated:
            self.send(stream, event.response())
        if not self.streams[stream]["reset"]:
            self.h2.end_stream(stream)
            self.flush()
        self.wait(lambda: self.streams[stream]["ended"])

    def reset_stream(self, stream):
        self.h2.reset_stream(stream)
        self.streams[stream]["ended"] = True
        self.flush()

    def close(self):
        self.socket.close()
