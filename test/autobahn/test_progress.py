"""Progress must reflect observed cases, without claiming compliance results."""
import importlib.util
import io
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "autobahn_progress", Path(__file__).resolve().parents[2] / "tools/autobahn_progress.py")
progress_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(progress_module)


class ProgressTest(unittest.TestCase):
    def test_only_known_cases_are_counted_once(self):
        output = io.StringIO()
        progress = progress_module.Progress("client", ["1.1.1", "6.4.1"], output)
        progress.read([
            "Running test case ID 1.1.1 for agent ixwebsocket from peer tcp4:host\n",
            "Running test case ID 1.1.1 for agent ixwebsocket from peer tcp4:host\n",
            "Running test case ID 9.9.9 for agent ixwebsocket from peer tcp4:host\n",
            "Running test case ID 6.4.1 for agent other\n",
        ])
        progress.display(final=True)
        self.assertIn("client: 1/2 cases started | 1.1.1", output.getvalue())
        self.assertNotIn("PASS", output.getvalue())
        self.assertNotIn("\033", output.getvalue())

    def test_terminal_updates_in_place(self):
        output = io.StringIO()
        output.isatty = lambda: True
        progress = progress_module.Progress("server", ["1.1.1"], output)
        progress.display()
        self.assertTrue(output.getvalue().startswith("\r\033[2K"))
        self.assertFalse(output.getvalue().endswith("\n"))
        progress.display(final=True)
        self.assertTrue(output.getvalue().endswith("\n"))

    def test_redirected_output_has_periodic_heartbeat(self):
        output = io.StringIO()
        with patch.object(progress_module.time, "monotonic", return_value=0) as clock:
            progress = progress_module.Progress("client", ["1.1.1"], output)
            progress.display()
            clock.return_value = 1
            progress.display()
            self.assertEqual(len(output.getvalue().splitlines()), 1)
            clock.return_value = 30
            progress.display()
            self.assertEqual(len(output.getvalue().splitlines()), 2)

    def test_timeout_stops_both_docker_helpers(self):
        class Process:
            def __init__(self):
                self.stdout = io.StringIO("")
                self.stderr = io.StringIO("")
                self.args = ["docker"]
                self.returncode = None
                self.terminated = False

            def poll(self):
                return self.returncode

            def terminate(self):
                self.terminated = True
                self.returncode = -15

            def wait(self, timeout):
                return self.returncode

        follower, waiter = Process(), Process()
        with patch.object(progress_module.subprocess, "Popen", side_effect=[follower, waiter]), \
                patch.object(progress_module.sys, "stdout", io.StringIO()):
            with self.assertRaises(subprocess.TimeoutExpired):
                progress_module.wait_with_progress("test", "logs", "client", ["1.1.1"], 0)
        self.assertTrue(follower.terminated)
        self.assertTrue(waiter.terminated)
        self.assertTrue(follower.stdout.closed)
        self.assertTrue(waiter.stdout.closed)


if __name__ == "__main__":
    unittest.main()
