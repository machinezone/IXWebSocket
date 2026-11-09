"""Display case progress from Autobahn's log stream without interpreting results."""
import re
import subprocess
import sys
import threading
import time


class Progress:
    def __init__(self, role, expected, stream=None):
        self.role = role
        self.expected = set(expected)
        self.stream = stream if stream is not None else sys.stdout
        self.tty = self.stream.isatty()
        self.started = time.monotonic()
        self.seen = set()
        self.current = "waiting for first case"
        self.lock = threading.Lock()
        self.last_line = None
        self.last_display = 0

    def read(self, lines):
        for line in lines:
            match = re.search(r"Running test case ID ([\d.]+) for agent ixwebsocket\b", line)
            if match and match.group(1) in self.expected:
                with self.lock:
                    self.current = match.group(1)
                    self.seen.add(self.current)

    def display(self, final=False):
        elapsed = int(time.monotonic() - self.started)
        with self.lock:
            count, current = len(self.seen), self.current
        line = "{}: {}/{} cases started | {} | {:02d}:{:02d} elapsed".format(
            self.role, count, len(self.expected), current, elapsed // 60, elapsed % 60)
        changed = self.last_line != (count, current)
        if final or changed or self.tty or elapsed - self.last_display >= 30:
            self.stream.write(("\r\033[2K" if self.tty else "") + line +
                              ("\n" if final or not self.tty else ""))
            self.stream.flush()
            self.last_line = (count, current)
            self.last_display = elapsed


def wait_with_progress(container, log_container, role, expected, timeout):
    progress = Progress(role, expected)
    follower = subprocess.Popen(
        ["docker", "logs", "--follow", log_container], stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True)
    reader = threading.Thread(target=progress.read, args=(follower.stdout,), daemon=True)
    reader.start()
    waiter = None
    try:
        waiter = subprocess.Popen(["docker", "wait", container], stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True)
        deadline = time.monotonic() + timeout
        while waiter.poll() is None:
            progress.display()
            if time.monotonic() >= deadline:
                raise subprocess.TimeoutExpired(waiter.args, timeout)
            try:
                waiter.wait(timeout=1)
            except subprocess.TimeoutExpired:
                pass
        output, error = waiter.communicate()
        if waiter.returncode:
            raise subprocess.CalledProcessError(waiter.returncode, waiter.args, output, error)
        return int(output.strip())
    finally:
        for process in (waiter, follower):
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=3)
        reader.join(timeout=3)
        follower.stdout.close()
        if waiter is not None:
            waiter.stdout.close()
            waiter.stderr.close()
        progress.display(final=True)
