"""Fast protocol/CLI tests. Performance runs are intentionally separate."""
import base64
import hashlib
import json
import socket
import subprocess
import sys
import threading
import unittest

WS = sys.argv.pop(1)


def exact(peer, count):
    result = b""
    while len(result) < count:
        part = peer.recv(count - len(result))
        if not part:
            raise EOFError("peer closed")
        result += part
    return result


class PushServer:
    def __init__(self, connections, early_close=False):
        self.connections = connections
        self.early_close = early_close
        self.error = None
        self.paths = []
        self.listener = socket.socket()
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(8)
        self.listener.settimeout(2)
        self.url = "ws://127.0.0.1:%d" % self.listener.getsockname()[1]
        self.thread = threading.Thread(target=self.serve, daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *args):
        self.thread.join(3)
        self.listener.close()
        if not args[0]:
            if self.thread.is_alive():
                raise AssertionError("push server did not finish")
            if self.error:
                raise self.error

    def serve(self):
        try:
            for _ in range(self.connections):
                with self.listener.accept()[0] as peer:
                    peer.settimeout(2)
                    request = b""
                    while not request.endswith(b"\r\n\r\n"):
                        request += exact(peer, 1)
                    lines = request.decode().split("\r\n")
                    self.paths.append(lines[0].split()[1])
                    headers = dict(line.split(": ", 1) for line in lines[1:] if ": " in line)
                    assert "Sec-WebSocket-Extensions" not in headers
                    key = headers["Sec-WebSocket-Key"] + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
                    accept = base64.b64encode(hashlib.sha1(key.encode()).digest()).decode()
                    reply = ("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                             "Connection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n" % accept).encode()
                    count = int(self.paths[-1].split("?")[0].split("/")[-1])
                    payload = b"\x81\x02hi" * (count // 2 if self.early_close else count)
                    if self.early_close:
                        payload += b"\x88\x02\x03\xe8"
                    peer.sendall(reply + payload)
                    first, second = exact(peer, 2)
                    assert first == 0x88 and second & 0x80
                    mask = exact(peer, 4)
                    data = exact(peer, second & 127)
                    data = bytes(value ^ mask[index % 4] for index, value in enumerate(data))
                    if not self.early_close:
                        peer.sendall(bytes([0x88, len(data)]) + data)
        except Exception as error:
            self.error = error


class BenchTest(unittest.TestCase):
    def run_bench(self, *args):
        return subprocess.run([WS, "bench", *args], capture_output=True, text=True, timeout=5)

    def test_modes_warmups_counts_and_json(self):
        with PushServer(6) as server:
            result = self.run_bench(server.url + "?token=test", "--messages", "64", "--runs", "2",
                                    "--warmup", "1", "--json", "--no-progress")
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads(result.stdout)
            self.assertEqual(len(report["trials"]), 4)
            self.assertEqual([r["callback"] for r in report["trials"]], ["ref", "legacy", "legacy", "ref"])
            for trial in report["trials"]:
                self.assertEqual(trial["messages"], 64)
                self.assertEqual(trial["payload_bytes"], 128)
                self.assertGreater(trial["messages_per_second"], 0)
            self.assertEqual(server.paths, ["/64?token=test"] * 6)
            self.assertEqual(len(report["summary"]), 2)

    def test_single_legacy_or_reference_trial(self):
        for mode in ["legacy", "ref"]:
            with self.subTest(mode=mode), PushServer(1) as server:
                result = self.run_bench(server.url, "--messages", "8", "--runs", "1",
                                        "--warmup", "0", "--callback", mode, "--no-progress")
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn(mode + ": median", result.stdout)

    def test_early_close_fails_without_hanging_or_reconnecting(self):
        with PushServer(1, early_close=True) as server:
            result = self.run_bench(server.url, "--messages", "64", "--runs", "1",
                                    "--warmup", "0", "--callback", "ref", "--json")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("32/64", result.stderr)
            self.assertEqual(result.stdout, "")

    def test_invalid_arguments_do_not_connect(self):
        for args in [("--messages", "0"), ("--callback", "unknown"), ("--runs", "0"), ("--timeout", "0")]:
            with self.subTest(args=args):
                result = self.run_bench("ws://127.0.0.1:1", *args)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Invalid benchmark", result.stderr)


if __name__ == "__main__":
    unittest.main()
