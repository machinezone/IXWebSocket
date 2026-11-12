"""Exercise the ws CLI against a local SOCKS5 peer without external services."""
import base64
import hashlib
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest


WS = sys.argv.pop(1)


def read_exact(stream, size):
    result = b""
    while len(result) < size:
        part = stream.recv(size - len(result))
        if not part:
            raise EOFError("Incomplete SOCKS5 request")
        result += part
    return result


class Proxy:
    def __init__(self, auth=False, reject=False):
        self.auth = auth
        self.reject = reject
        self.error = None
        self.listener = socket.socket()
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(1)
        self.listener.settimeout(3)
        self.url = "socks5h://127.0.0.1:%d" % self.listener.getsockname()[1]
        self.thread = threading.Thread(target=self.serve, daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *args):
        self.thread.join(4)
        self.listener.close()
        if not args[0]:
            if self.thread.is_alive():
                raise AssertionError("Proxy did not finish")
            if self.error:
                raise self.error

    def serve(self):
        try:
            with self.listener.accept()[0] as peer:
                peer.settimeout(3)
                version, count = read_exact(peer, 2)
                assert version == 5
                methods = read_exact(peer, count)
                assert (2 if self.auth else 0) in methods
                peer.sendall(b"\x05\x02" if self.auth else b"\x05\x00")
                if self.auth:
                    assert read_exact(peer, 1) == b"\x01"
                    user = read_exact(peer, read_exact(peer, 1)[0])
                    password = read_exact(peer, read_exact(peer, 1)[0])
                    assert (user, password) == (b"user", b"pass:word")
                    peer.sendall(b"\x01\x00")
                version, command, reserved, self.atyp = read_exact(peer, 4)
                assert (version, command, reserved) == (5, 1, 0)
                if self.atyp == 3:
                    self.host = read_exact(peer, read_exact(peer, 1)[0]).decode()
                else:
                    family, size = (socket.AF_INET, 4) if self.atyp == 1 else (socket.AF_INET6, 16)
                    self.host = socket.inet_ntop(family, read_exact(peer, size))
                self.port = int.from_bytes(read_exact(peer, 2), "big")
                peer.sendall(b"\x05" + (b"\x05" if self.reject else b"\x00") +
                             b"\x00\x01\x7f\x00\x00\x01\x00\x50")
                if self.reject:
                    return
                request = b""
                while b"\r\n\r\n" not in request:
                    request += read_exact(peer, 1)
                    assert len(request) < 16384
                if b"upgrade: websocket" in request.lower():
                    key = next(line.split(b":", 1)[1].strip() for line in request.split(b"\r\n")
                               if line.lower().startswith(b"sec-websocket-key:"))
                    accept = base64.b64encode(hashlib.sha1(
                        key + b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11").digest())
                    peer.sendall(b"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                                 b"Connection: Upgrade\r\nSec-WebSocket-Accept: " + accept +
                                 b"\r\n\r\n\x88\x02\x03\xe8")
                    peer.recv(1024)
                else:
                    peer.sendall(b"HTTP/1.1 200 OK\r\nContent-Length: 7\r\n"
                                 b"Connection: close\r\n\r\nproxied")
        except Exception as error:
            self.error = error


class WsProxyTest(unittest.TestCase):
    def run_ws(self, *args):
        return subprocess.run([WS, *args], capture_output=True, text=True, timeout=5)

    def test_http_dns_and_authentication(self):
        for scheme, host in [("socks5h", "origin.invalid"), ("socks5", "localhost"),
                             ("socks5h", "127.0.0.1"), ("socks5h", "[::1]")]:
            for auth in [False, True]:
                with self.subTest(scheme=scheme, host=host, auth=auth), Proxy(auth) as proxy:
                    args = ["curl", "http://%s:8080/" % host, "--proxy",
                            proxy.url.replace("socks5h", scheme)]
                    if auth:
                        args += ["--proxy-user", "user:pass:word"]
                    result = self.run_ws(*args)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn("Status: 200", result.stdout + result.stderr)
                self.assertEqual(proxy.port, 8080)
                self.assertEqual(proxy.atyp == 3, host == "origin.invalid")
                if host == "origin.invalid":
                    self.assertEqual(proxy.host, host)

    def test_connect(self):
        with tempfile.TemporaryDirectory() as directory, Proxy(auth=True) as proxy:
            log = Path(directory) / "ws.log"
            with subprocess.Popen([WS, "connect", "ws://origin.invalid:8080/", "-d", "-x",
                                   "--proxy", proxy.url, "--proxy-user", "user:pass:word",
                                   "--logfile", str(log)], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE) as process:
                try:
                    deadline = time.monotonic() + 3
                    while time.monotonic() < deadline:
                        if log.exists() and "ws_connect: connected" in log.read_text():
                            break
                        time.sleep(0.01)
                    else:
                        self.fail("No WebSocket Open event: " + (log.read_text() if log.exists() else ""))
                    process.communicate(input=b"", timeout=3)
                    self.assertEqual(process.returncode, 0)
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.communicate()
        self.assertEqual(proxy.host, "origin.invalid")

    def test_rejected_proxy(self):
        with Proxy(reject=True) as proxy:
            result = self.run_ws("curl", "http://origin.invalid/", "--proxy", proxy.url)
            self.assertIn("SOCKS5 CONNECT failed", result.stdout + result.stderr)
            self.assertNotIn("Status: 200", result.stdout + result.stderr)

    def test_invalid_options(self):
        for command in ["curl", "connect"]:
            for url in ["", "http://localhost:1080", "socks4://localhost", "socks5://",
                        "socks5://localhost:0", "socks5://localhost:65536",
                        "socks5://localhost:bad", "socks5://localhost/path",
                        "socks5://localhost?query", "socks5://localhost#fragment",
                        "socks5://user:secret@localhost", "socks5://bad host"]:
                with self.subTest(command=command, proxy=url):
                    result = self.run_ws(command, "ws://origin.invalid", "--proxy", url)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("Invalid --proxy:", result.stderr)
                    self.assertNotIn("secret", result.stderr)
            for user in ["", "missing-colon", ":pass", "user:", "u" * 256 + ":pass"]:
                result = self.run_ws(command, "ws://origin.invalid", "--proxy",
                                     "socks5://localhost", "--proxy-user", user)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Invalid --proxy-user:", result.stderr)
            result = self.run_ws(command, "ws://origin.invalid", "--proxy-user", "user:pass")
            self.assertNotEqual(result.returncode, 0)

    def test_network_framework_reports_unsupported_tls_proxy(self):
        if "ssl/Network.framework" not in self.run_ws("--version").stdout:
            self.skipTest("Requires the Network.framework backend")
        result = self.run_ws("curl", "https://origin.invalid", "--proxy",
                             "socks5h://127.0.0.1:1080")
        self.assertIn("SOCKS5 is not supported by the Network.framework backend",
                      result.stdout + result.stderr)


if __name__ == "__main__":
    # DEBUG intentionally logs the entire command line; keep test credentials out of it.
    os.environ.pop("DEBUG", None)
    unittest.main()
