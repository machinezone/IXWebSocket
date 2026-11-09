#!/usr/bin/env python3
"""Run Autobahn against the current source in isolated Docker containers."""
import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import uuid

from check_autobahn import check_report
from autobahn_progress import wait_with_progress

ROOT = Path(__file__).resolve().parents[1]
SUITE = ("crossbario/autobahn-testsuite:25.10.1@sha256:"
         "519915fb568b04c9383f70a1c405ae3ff44ab9e35835b085239c258b6fac3074")
TARGET = "ixwebsocket-autobahn:local"


def command(args, **kwargs):
    return subprocess.run(args, check=True, text=True, **kwargs)


class Runner:
    def __init__(self, output, timeout, interactive=False):
        self.output = output
        self.timeout = timeout
        self.interactive = interactive
        self.network = "ix-autobahn-" + uuid.uuid4().hex[:12]
        self.containers = []
        self.image = None
        self.user = ["--user", "{}:{}".format(os.getuid(), os.getgid())]

    def start(self, name, image, args, suite=False, alias=None):
        container = self.network + "-" + name
        self.containers.append(container)
        options = ["docker", "run", "--detach", "--init", "--name", container,
                   "--network", self.network, "--log-driver", "json-file",
                   "--log-opt", "max-size=10m", "--log-opt", "max-file=1"] + self.user
        if alias:
            options += ["--network-alias", alias]
        if suite:
            options += ["--env", "PYTHONUNBUFFERED=1", "--platform", "linux/amd64", "--volume",
                        str(self.output / "config") + ":/config:ro", "--volume",
                        str(ROOT / "test/autobahn") + ":/harness:ro", "--volume",
                        str(self.output / "reports") + ":/reports"]
        command(options + [image] + args, stdout=subprocess.DEVNULL)
        return container

    def wait(self, container):
        result = command(["docker", "wait", container], capture_output=True,
                         timeout=self.timeout + 30)
        return int(result.stdout.strip())

    def ready(self, peer, port):
        probe = (
            "import socket,time\n"
            "deadline=time.monotonic()+30\n"
            "while True:\n"
            " try:\n"
            "  socket.create_connection((%r,%r),timeout=1).close(); break\n"
            " except OSError:\n"
            "  if time.monotonic()>deadline: raise\n"
            "  time.sleep(0.2)\n" % (peer, port))
        container = self.start("probe-" + str(port), self.image, ["python3", "-c", probe])
        status = command(["docker", "wait", container], capture_output=True, timeout=45)
        if int(status.stdout.strip()):
            raise RuntimeError("Server did not become ready: " + peer)

    def collect_logs(self):
        for name in self.containers:
            with (self.output / "logs" / (name + ".log")).open("w") as log:
                try:
                    subprocess.run(["docker", "logs", name], stdout=log,
                                   stderr=subprocess.STDOUT, text=True, timeout=30)
                except (OSError, subprocess.SubprocessError) as error:
                    print("Cannot collect logs for {}: {}".format(name, error), file=sys.stderr)

    def cleanup(self):
        try:
            self.collect_logs()
        finally:
            commands = [["docker", "rm", "--force", name] for name in self.containers]
            commands.append(["docker", "network", "rm", self.network])
            for args in commands:
                try:
                    subprocess.run(args, stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL, timeout=30)
                except (OSError, subprocess.SubprocessError) as error:
                    print("Cleanup failed: {}: {}".format(args, error), file=sys.stderr)

    def run(self, roles, patterns):
        # Each run owns these generated directories. Never reuse old reports.
        for name in ("reports", "logs", "config"):
            directory = self.output / name
            if directory.exists():
                shutil.rmtree(directory)
            directory.mkdir(parents=True)
        for name in ("fuzzingserver.json", "fuzzingclient.json"):
            spec = json.loads((ROOT / "test/autobahn" / name).read_text())
            spec["cases"] = patterns
            (self.output / "config" / name).write_text(json.dumps(spec, indent=2) + "\n")
        print("Building current IXWebSocket source (zlib enabled)...", flush=True)
        image_id = self.output / "image-id"
        if image_id.exists():
            image_id.unlink()
        with (self.output / "logs/build.log").open("w") as log:
            command(["docker", "build", "--file", str(ROOT / "docker/Dockerfile.autobahn"),
                     "--iidfile", str(image_id), "--tag", TARGET, str(ROOT)],
                    stdout=log, stderr=subprocess.STDOUT)
        self.image = image_id.read_text().strip()
        command(["docker", "pull", "--platform", "linux/amd64", SUITE],
                stdout=subprocess.DEVNULL)
        expected_json = command([
            "docker", "run", "--rm", "--platform", "linux/amd64", "--volume",
            str(self.output / "config") + ":/config:ro", "--volume",
            str(ROOT / "test/autobahn") + ":/harness:ro", SUITE, "python",
            "/harness/expected_cases.py", "/config/fuzzingserver.json"],
            capture_output=True).stdout
        expected = json.loads(expected_json)
        if not expected:
            raise RuntimeError("Autobahn case selection is empty")
        (self.output / "expected.json").write_text(json.dumps(expected, indent=2) + "\n")
        (self.output / "selection.json").write_text(json.dumps({
            "suite_image": SUITE, "target_image": self.image, "roles": roles,
            "patterns": patterns, "expected_count": len(expected)}, indent=2) + "\n")
        command(["docker", "network", "create", self.network], stdout=subprocess.DEVNULL)
        failed = False
        results = []
        try:
            for role in roles:
                print("Running {} compliance: {} cases".format(role, len(expected)), flush=True)
                if role == "client":
                    peer = self.start("fuzzer", SUITE, [
                        "wstest", "-m", "fuzzingserver", "-s", "/config/fuzzingserver.json"],
                        suite=True, alias="autobahn")
                    self.ready("autobahn", 9001)
                    test = self.start("client", self.image, [
                        "timeout", "--kill-after=10s", str(self.timeout),
                        "ws", "autobahn", "-q", "--url", "ws://autobahn:9001"])
                else:
                    peer = self.start("echo", self.image, [
                        "ws", "echo_server", "--host", "0.0.0.0", "--port", "9002", "-q"],
                        alias="ixwebsocket")
                    self.ready("ixwebsocket", 9002)
                    test = self.start("server", SUITE, [
                        "timeout", "--kill-after=10s", str(self.timeout),
                        "wstest", "-m", "fuzzingclient", "-s", "/config/fuzzingclient.json"],
                        suite=True)
                if self.interactive:
                    status = wait_with_progress(test, peer if role == "client" else test,
                                                role, expected, self.timeout + 30)
                else:
                    status = self.wait(test)
                if status:
                    print("{} runner failed with exit {}".format(role, status), flush=True)
                    failed = True
                report_file = self.output / "reports" / role / "index.json"
                try:
                    errors = check_report(json.loads(report_file.read_text()), expected)
                except (OSError, ValueError, TypeError) as error:
                    errors = [str(error)]
                for error in errors:
                    print("{}: {}".format(role, error), flush=True)
                failed = failed or bool(errors)
                results.append((role, status == 0 and not errors))
                print("{}: {} expected cases, {} report failures".format(
                    role, len(expected), len(errors)), flush=True)
                self.collect_logs()
                command(["docker", "stop", "--time", "5", peer], stdout=subprocess.DEVNULL)
        finally:
            self.cleanup()
        if self.interactive:
            print("\nAutobahn summary", flush=True)
            for role, passed in results:
                print("  {}: {} ({} expected cases)".format(
                    role, "PASS" if passed else "FAIL", len(expected)), flush=True)
            print("Reports: " + str(self.output / "reports"), flush=True)
        return int(failed)


def interrupted(signum, frame):
    raise KeyboardInterrupt


def main():
    signal.signal(signal.SIGTERM, interrupted)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--interactive", action="store_true",
                        help="Show live case progress and elapsed time")
    parser.add_argument("--role", choices=("client", "server", "both"), default="both")
    parser.add_argument("--cases", default="*", help="Comma-separated patterns for local debugging")
    parser.add_argument("--timeout", type=int, default=1800, help="Maximum seconds per role")
    parser.add_argument("--output", type=Path, default=ROOT / "build/autobahn")
    args = parser.parse_args()
    output = args.output.resolve()
    try:
        relative = output.relative_to((ROOT / "build").resolve())
        if not relative.parts:
            raise ValueError("Use a subdirectory of build")
    except ValueError:
        parser.error("Keep generated reports under the repository's build directory")
    if args.timeout <= 0:
        parser.error("Timeout must be positive")
    if os.environ.get("CI") and args.cases != "*":
        parser.error("CI always runs the full case selection")
    output.mkdir(parents=True, exist_ok=True)
    lock = output / ".lock"
    try:
        with lock.open("x") as handle:
            handle.write(str(os.getpid()))
    except FileExistsError:
        parser.error("Another run owns this output directory: " + str(lock))
    roles = ["client", "server"] if args.role == "both" else [args.role]
    try:
        return Runner(output, args.timeout, args.interactive).run(roles, args.cases.split(","))
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print("Autobahn infrastructure failure: " + str(error), file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Autobahn interrupted", file=sys.stderr)
        return 130
    finally:
        lock.unlink()


if __name__ == "__main__":
    raise SystemExit(main())
