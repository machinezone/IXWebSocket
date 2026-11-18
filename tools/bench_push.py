#!/usr/bin/env python3
"""Optional autoroute push fixture using only the Python standard library.

Batching frames reduces sender overhead when measuring client receive throughput.
This is a benchmark fixture, not a general-purpose WebSocket server.
"""
import argparse
import base64
import hashlib
import socketserver
import struct


def exact(peer, size):
    data = b""
    while len(data) < size:
        part = peer.recv(size - len(data))
        if not part:
            raise EOFError("client disconnected")
        data += part
    return data


class Push(socketserver.BaseRequestHandler):
    def handle(self):
        peer = self.request
        peer.settimeout(30)
        try:
            request = b""
            while not request.endswith(b"\r\n\r\n"):
                request += exact(peer, 1)
                if len(request) > 65536:
                    raise ValueError("oversized handshake")
            lines = request.decode("ascii").split("\r\n")
            count = int(lines[0].split()[1].split("?")[0].rstrip("/").rsplit("/", 1)[-1])
            if count <= 0 or count > 2147483647:
                raise ValueError("invalid count")
            headers = {line.split(":", 1)[0].lower(): line.split(":", 1)[1].strip()
                       for line in lines[1:] if ":" in line}
            key = headers["sec-websocket-key"]
            accept = base64.b64encode(hashlib.sha1(
                (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
            peer.sendall(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                          "Connection: Upgrade\r\nSec-WebSocket-Accept: " + accept + "\r\n\r\n").encode())
            while count:
                frames = min(count, self.server.batch_frames)
                peer.sendall(self.server.block[:frames * self.server.frame_size])
                count -= frames
            while True:
                first, second = exact(peer, 2)
                if not (second & 128) or (second & 127) > 125:
                    raise ValueError("expected masked control frame")
                mask = exact(peer, 4)
                body = exact(peer, second & 127)
                body = bytes(value ^ mask[index % 4] for index, value in enumerate(body))
                if (first & 15) == 8:
                    peer.sendall(bytes([0x88, len(body)]) + body)
                    return
                if (first & 15) == 9:
                    peer.sendall(bytes([0x8a, len(body)]) + body)
        except (OSError, EOFError, ValueError, KeyError, IndexError, UnicodeError):
            return


class PushServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, address, payload, batch_frames):
        size = len(payload)
        length = bytes([size]) if size < 126 else (
            b"\x7e" + struct.pack("!H", size) if size <= 65535 else b"\x7f" + struct.pack("!Q", size))
        frame = b"\x81" + length + payload
        self.frame_size = len(frame)
        self.batch_frames = batch_frames
        self.block = frame * batch_frames
        super().__init__(address, Push)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8008)
    parser.add_argument("--payload", default="hello world")
    parser.add_argument("--batch-frames", type=int, default=8192)
    args = parser.parse_args()
    payload = args.payload.encode("utf-8")
    if args.batch_frames < 1 or (len(payload) + 10) * args.batch_frames > 16 * 1024 * 1024:
        parser.error("batch must contain at least one frame and fit within 16 MiB")
    with PushServer((args.host, args.port), payload, args.batch_frames) as server:
        print("Autoroute fixture listening on %s:%d" % server.server_address, flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
