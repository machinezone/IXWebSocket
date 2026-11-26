#!/usr/bin/env python3
"""Independent Python H2/WebSocket peer for native TLS-backend interoperability."""
import argparse
from collections import deque
from pathlib import Path
import socket
import ssl
import subprocess
import threading
import traceback

from h2.config import H2Configuration
from h2.connection import H2Connection
from h2.events import DataReceived, RequestReceived, StreamEnded, StreamReset
from h2.settings import Settings, SettingCodes
from wsproto import Connection, ConnectionType
from wsproto.connection import ConnectionState
from wsproto.events import BytesMessage, CloseConnection, Ping, TextMessage
from wsproto.extensions import PerMessageDeflate


def serve(listener, context, errors):
    try:
        for connection_index in range(7):
            raw, _ = listener.accept()
            with context.wrap_socket(raw, server_side=True) as transport:
                transport.settimeout(10)
                assert transport.selected_alpn_protocol() == "h2"
                h2 = H2Connection(config=H2Configuration(client_side=False, header_encoding="utf-8"))
                h2.local_settings = Settings(client=False, initial_values={
                    SettingCodes.ENABLE_CONNECT_PROTOCOL: 1})
                if connection_index == 6:
                    h2.local_settings[SettingCodes.INITIAL_WINDOW_SIZE] = 0
                    h2.local_settings.acknowledge()
                del h2.local_settings[SettingCodes.ENABLE_PUSH]
                h2.initiate_connection()
                transport.sendall(h2.data_to_send())
                ws, outgoing, chunks, pending = None, deque(), [], deque()
                ending, duplex = False, False

                def receive(event):
                    nonlocal ending
                    h2.acknowledge_received_data(event.flow_controlled_length, 1)
                    if not event.data:
                        return
                    ws.receive_data(event.data)
                    for frame in ws.events():
                        if isinstance(frame, (TextMessage, BytesMessage)):
                            chunks.append(frame.data)
                            if frame.message_finished:
                                value = ("" if isinstance(frame, TextMessage) else b"").join(chunks)
                                chunks.clear()
                                if value == "close-from-server":
                                    outgoing.append(ws.send(CloseConnection(code=4000, reason="server close test")))
                                else:
                                    outgoing.append(ws.send(type(frame)(data=value)))
                        elif isinstance(frame, Ping):
                            outgoing.append(ws.send(frame.response()))
                        elif isinstance(frame, CloseConnection):
                            if ws.state == ConnectionState.REMOTE_CLOSING:
                                outgoing.append(ws.send(frame.response()))
                            ending = True

                while True:
                    data = transport.recv(65536)
                    if not data:
                        break
                    for event in h2.receive_data(data):
                        if isinstance(event, RequestReceived):
                            headers = dict(event.headers)
                            assert headers[":method"] == "CONNECT" and headers[":protocol"] == "websocket"
                            assert "sec-websocket-key" not in headers
                            extensions = []
                            response = [(":status", "200")]
                            if "sec-websocket-extensions" in headers:
                                extension = PerMessageDeflate()
                                accepted = extension.accept(headers["sec-websocket-extensions"])
                                assert accepted is not None
                                response.append(("sec-websocket-extensions", "permessage-deflate; " + accepted))
                                extensions.append(extension)
                            ws = Connection(ConnectionType.SERVER, extensions=extensions)
                            h2.send_headers(1, response)
                            duplex = "x-h2-duplex" in headers
                            if duplex:
                                # Fill both directions before consuming application data.
                                # Keep the queued incoming DATA bounded by the H2 window.
                                state, payload = 0x12345678, bytearray()
                                for _ in range(256 * 1024):
                                    state ^= (state << 13) & 0xffffffff
                                    state ^= state >> 17
                                    state ^= (state << 5) & 0xffffffff
                                    payload.append(state & 255)
                                outgoing.append(ws.send(BytesMessage(data=bytes(payload))))
                        elif isinstance(event, DataReceived):
                            if duplex and outgoing:
                                pending.append(event)
                            else:
                                receive(event)
                        elif isinstance(event, (StreamEnded, StreamReset)):
                            ending = True
                    while True:
                        while outgoing:
                            size = min(len(outgoing[0]), h2.local_flow_control_window(1), h2.max_outbound_frame_size)
                            if not size:
                                break
                            h2.send_data(1, outgoing[0][:size])
                            outgoing[0] = outgoing[0][size:]
                            if not outgoing[0]:
                                outgoing.popleft()
                        if outgoing or not pending:
                            break
                        while pending:
                            receive(pending.popleft())
                    if ending and not outgoing:
                        h2.end_stream(1)
                    transport.sendall(h2.data_to_send())
                    if ending and not outgoing:
                        break
    except Exception as error:
        errors.append(error)
        traceback.print_exc()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("client", type=Path)
    parser.add_argument("certificate", type=Path)
    parser.add_argument("key", type=Path)
    parser.add_argument("ca", type=Path)
    parser.add_argument("--reject-alpn", action="store_true")
    args = parser.parse_args()
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(args.certificate, args.key)
    context.set_alpn_protocols(["h2"])
    errors = []
    if args.reject_alpn:
        context.set_alpn_protocols(["http/1.1"])
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        listener.settimeout(10)
        def reject():
            try:
                raw, _ = listener.accept()
                with context.wrap_socket(raw, server_side=True) as transport:
                    transport.settimeout(5)
                    assert not transport.recv(65536), "Client sent bytes without negotiating h2"
            except Exception as error:
                errors.append(error)
        worker = threading.Thread(target=reject if args.reject_alpn else serve,
                                  args=() if args.reject_alpn else (listener, context, errors),
                                  daemon=True)
        worker.start()
        if args.reject_alpn:
            result = subprocess.run([str(args.client), "wss://127.0.0.1:" + str(listener.getsockname()[1]) + "/native",
                                     str(args.ca), "off", "client"], timeout=10, capture_output=True)
            assert result.returncode != 0, "Client accepted HTTP/1.1 fallback"
            print("PASS native client rejects missing ALPN h2 without sending HTTP bytes")
        else:
            for compression in ("off", "on"):
                for close in ("client", "server", "duplex"):
                    subprocess.run([str(args.client), "wss://127.0.0.1:" + str(listener.getsockname()[1]) + "/native",
                                    str(args.ca), compression, close], check=True, timeout=10)
        if not args.reject_alpn:
            subprocess.run([str(args.client), "wss://127.0.0.1:" + str(listener.getsockname()[1]) + "/native",
                            str(args.ca), "off", "cancel"], check=True, timeout=10)
        worker.join(timeout=10)
        assert not worker.is_alive(), "TLS peer did not stop"
        if errors:
            raise errors[0]


if __name__ == "__main__":
    main()
