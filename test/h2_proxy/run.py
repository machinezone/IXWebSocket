#!/usr/bin/env python3
"""Exercise TLS/H2 Extended CONNECT -> reverse proxy -> HTTP/1.1 IXWebSocket."""
import argparse
import json
from pathlib import Path
import random
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import traceback
import urllib.request

from wsproto.events import BytesMessage, Ping, Pong, TextMessage
from client import Client

ROOT = Path(__file__).resolve().parents[2]


def wait_for(read, processes, timeout=10):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for process in processes:
            if process.poll() is not None:
                raise RuntimeError("Test process exited with status " + str(process.returncode))
        try:
            result = read()
            if result:
                return result
        except (OSError, ValueError):
            pass
        time.sleep(0.05)
    raise TimeoutError("Test services did not become ready")


def start_envoy(args, output, certificate, key, port, processes, logs):
    config = (ROOT / "test/h2_proxy/envoy.yaml").read_text()
    for token, value in {"CERT": str(certificate), "KEY": str(key),
                         "ACCESS_LOG": str(output / "access.log"),
                         "ADMIN_LOG": str(output / "admin.log"), "BACKEND_PORT": port}.items():
        config = config.replace("@" + token + "@", json.dumps(value))
    config_file = output / "envoy.yaml"
    config_file.write_text(config)
    admin_file = output / "admin-address"
    log = (output / "envoy.log").open("w")
    logs.append(log)
    proxy = subprocess.Popen([args.envoy, "-c", str(config_file), "--disable-hot-restart",
                              "--concurrency", "1", "--admin-address-path", str(admin_file)],
                             stdout=log, stderr=subprocess.STDOUT)
    processes.append(proxy)

    def proxy_port():
        address = admin_file.read_text().strip()
        with urllib.request.urlopen("http://" + address + "/listeners?format=json", timeout=1) as response:
            listeners = json.load(response)
        return listeners["listener_statuses"][0]["local_address"]["socket_address"]["port_value"]
    return wait_for(proxy_port, processes)


def start_nghttpx(args, output, certificate, key, backend_port, processes, logs):
    # In Docker this loopback namespace contains only the test services.
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    log = (output / "nghttpx.log").open("w")
    logs.append(log)
    process = subprocess.Popen(
        [args.nghttpx, "--frontend=127.0.0.1," + str(port),
         "--backend=127.0.0.1," + str(backend_port), "--workers=1",
         "--accesslog-file=" + str(output / "access.log"),
         "--errorlog-file=/dev/stderr", str(key), str(certificate)],
        stdout=log, stderr=subprocess.STDOUT)
    processes.append(process)
    return port


def run(args):
    output = ROOT / "build/h2-proxy/runs"
    output.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=output))
    print("Logs and report: " + str(output), flush=True)
    processes, logs = [], []
    report = {"passed": [], "alpn": None, "proxy": args.proxy}
    started = time.monotonic()
    client = None
    try:
        version = subprocess.run([getattr(args, args.proxy), "--version"], check=True,
                                 capture_output=True, text=True, timeout=5)
        report["proxy_version"] = (version.stdout + version.stderr).strip()
        backend_log = output / "backend.log"
        log = backend_log.open("w")
        logs.append(log)
        backend = subprocess.Popen([str(args.server)], stdin=subprocess.PIPE,
                                   stdout=log, stderr=subprocess.STDOUT, text=True)
        processes.append(backend)

        def backend_port():
            for line in backend_log.read_text().splitlines():
                if line.startswith("READY "):
                    return int(line.split()[1])
        port = wait_for(backend_port, processes)
        ca, ca_key = output / "ca.pem", output / "ca.key"
        certificate, key = output / "server.pem", output / "server.key"
        config = output / "openssl.cnf"
        config.write_text("""[req]
        distinguished_name = dn
        x509_extensions = extensions
        prompt = no
        [dn]
        CN = IXWebSocket-H2-Test-CA
        [extensions]
        subjectAltName = DNS:localhost,IP:127.0.0.1
        basicConstraints = critical,CA:TRUE
        keyUsage = critical,digitalSignature,keyEncipherment,keyCertSign
        subjectKeyIdentifier = hash
        authorityKeyIdentifier = keyid:always
        """)
        with (output / "certificate.log").open("w") as log:
            subprocess.run([args.openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                            "-days", "1", "-config", str(config), "-keyout", str(ca_key),
                            "-out", str(ca)], check=True, stdout=log,
                           stderr=subprocess.STDOUT, timeout=15)
        leaf_config = output / "server.cnf"
        leaf_config.write_text("""[req]
        distinguished_name = dn
        prompt = no
        [dn]
        CN = localhost
        [extensions]
        basicConstraints = critical,CA:FALSE
        keyUsage = critical,digitalSignature,keyEncipherment
        extendedKeyUsage = serverAuth
        subjectAltName = DNS:localhost,IP:127.0.0.1
        subjectKeyIdentifier = hash
        authorityKeyIdentifier = keyid:always
        """)
        with (output / "certificate.log").open("a") as log:
            subprocess.run([args.openssl, "req", "-new", "-newkey", "rsa:2048", "-nodes",
                            "-config", str(leaf_config), "-keyout", str(key),
                            "-out", str(output / "server.csr")], check=True,
                           stdout=log, stderr=subprocess.STDOUT, timeout=15)
            subprocess.run([args.openssl, "x509", "-req", "-days", "1", "-CA", str(ca),
                            "-CAkey", str(ca_key), "-CAcreateserial", "-extfile", str(leaf_config),
                            "-extensions", "extensions", "-in", str(output / "server.csr"),
                            "-out", str(certificate)], check=True, stdout=log,
                           stderr=subprocess.STDOUT, timeout=15)
        if args.proxy == "envoy":
            port = start_envoy(args, output, certificate, key, port, processes, logs)
        else:
            port = start_nghttpx(args, output, certificate, key, port, processes, logs)
        client = wait_for(lambda: Client(port, ca), processes)
        if args.native_client:
            for compression in ("off", "on"):
                for close in ("client", "server"):
                    with (output / ("native-" + compression + "-" + close + ".log")).open("w") as log:
                        subprocess.run([str(args.native_client), "wss://127.0.0.1:" + str(port) + "/native",
                                        str(ca), compression, close], stdout=log,
                                       stderr=subprocess.STDOUT, timeout=15, check=True)
                    print("PASS native IXWebSocket H2 compression=" + compression + " close=" + close, flush=True)
            report["native_client"] = "4 combinations passed"
            for reject in (False, True):
                with (output / ("native-peer-reject.log" if reject else "native-peer.log")).open("w") as log:
                    command = [sys.executable, str(ROOT / "test/h2_proxy/native_peer.py"),
                               str(args.native_client), str(certificate), str(key), str(ca)]
                    if reject:
                        command.append("--reject-alpn")
                    subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT, timeout=45)
            report["independent_native_peer"] = "6 exchange combinations, blocked-send cancellation, and ALPN rejection passed"
            print("PASS native H2 client: independent peer, bidirectional backpressure and ALPN rejection", flush=True)
        report["alpn"] = "h2"
        report["python_downstream_connections"] = 1
        report["downstream_connections"] = 5 if args.native_client else 1

        def passed(name):
            print("PASS " + name, flush=True)
            report["passed"].append(name)

        for compression in (False, True):
            stream = client.open("/echo?compression=" + str(compression).lower(), compression)
            passed("Extended CONNECT returns 200; compression=" + str(compression))
            for payload in ("", "Bonjour — 中文 👋", b"", b"\x00\xff\x80binary"):
                client.send(stream, TextMessage(data=payload) if isinstance(payload, str)
                            else BytesMessage(data=payload))
                client.message(stream, payload)
            passed("empty, Unicode and binary messages; compression=" + str(compression))
            client.send(stream, TextMessage(data="fragment ", message_finished=False))
            client.send(stream, TextMessage(data="é", message_finished=True))
            client.message(stream, "fragment é")
            client.send(stream, Ping(payload=b"proxy-ping"))
            event = client.event(stream)
            if not isinstance(event, Pong) or event.payload != b"proxy-ping":
                raise RuntimeError("Ping/pong payload was not preserved")
            payload = random.Random(0).randbytes(256 * 1024)
            client.send(stream, BytesMessage(data=payload))
            client.message(stream, payload)
            passed("fragmentation, ping/pong and 256 KiB flow control; compression=" + str(compression))
            client.close_stream(stream)
            passed("client close code/reason and H2 stream retirement; compression=" + str(compression))

        streams = [client.open("/multiplex/" + str(i)) for i in range(3)]
        for i, stream in enumerate(streams):
            client.send(stream, TextMessage(data="stream-" + str(i)))
        for i, stream in enumerate(streams):
            client.message(stream, "stream-" + str(i))
        client.close_stream(streams[0])
        client.send(streams[1], TextMessage(data="survives-close"))
        client.message(streams[1], "survives-close")
        client.reset_stream(streams[2])
        client.send(streams[1], TextMessage(data="survives-reset"))
        client.message(streams[1], "survives-reset")
        client.close_stream(streams[1], server_initiated=True)
        passed("multiplexing, isolated close/reset and server close code/reason")
        stream = client.open("/after-close")
        client.send(stream, TextMessage(data="still connected"))
        client.message(stream, "still connected")
        client.close_stream(stream)
        passed("new WebSocket after earlier streams close on the same H2 connection")
        report["outbound_flow_control_waits"] = client.flow_control_waits
        if not client.flow_control_waits:
            raise RuntimeError("Test did not exercise HTTP/2 send flow control")
        transcript = backend_log.read_text()
        for path in ("/echo?compression=false", "/echo?compression=true",
                     "/multiplex/0", "/multiplex/1", "/multiplex/2", "/after-close"):
            if "OPEN " + path + "\n" not in transcript:
                raise RuntimeError("Backend did not receive the original path: " + path)
        passed("HTTP/1.1 backend receives original paths, queries and headers")
        report["success"] = True
    except Exception as error:
        report["success"] = False
        report["error"] = str(error)
        print("FAIL " + str(error), file=sys.stderr)
        with (output / "client.log").open("w") as log:
            traceback.print_exc(file=log)
    finally:
        if client:
            client.close()
        for process in reversed(processes):
            if process.poll() is None:
                if process is backend:
                    # EOF asks the fixture to stop and join its workers.
                    try:
                        process.stdin.close()
                    except OSError:
                        pass
                else:
                    process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        for log in logs:
            log.close()
        if processes:
            report["backend_exit_code"] = processes[0].returncode
            if processes[0].returncode != 0:
                report["success"] = False
                report.setdefault("error", "Backend did not stop cleanly")
        if report.get("success") and args.native_client:
            access = (output / "access.log").read_text().splitlines()
            native_h2 = [line for line in access if "HTTP/2" in line and
                         ("ixwebsocket/" in line if args.proxy == "nghttpx" else "/native" in line)]
            if len(native_h2) != 4:
                report["success"] = False
                report["error"] = "Proxy did not log four native HTTP/2 connections"
                print("FAIL " + report["error"], file=sys.stderr)
            else:
                report["native_h2_connections_verified"] = 4
                print("PASS proxy access log confirms all native clients used HTTP/2", flush=True)
        report["elapsed_seconds"] = round(time.monotonic() - started, 3)
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return 0 if report.get("success") else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", type=Path, default=ROOT / "build/h2-proxy/h2_proxy_server")
    parser.add_argument("--native-client", type=Path)
    parser.add_argument("--proxy", choices=("nghttpx", "envoy"), default="nghttpx")
    parser.add_argument("--nghttpx", default="nghttpx")
    parser.add_argument("--envoy", default="envoy")
    parser.add_argument("--openssl", default="openssl")
    args = parser.parse_args()
    if not shutil.which(getattr(args, args.proxy)):
        parser.error(args.proxy + " is required; see docs/h2-proxy.md")
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(130))
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
