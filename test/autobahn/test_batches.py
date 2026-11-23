"""Batching must preserve the full selected compliance suite without duplicates."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from run_autobahn import case_batches


class BatchesTest(unittest.TestCase):
    def test_full_suite_runs_once_per_role(self):
        expected = ["case-" + str(i) for i in range(517)]
        runs = list(case_batches(["client", "server"], expected))
        self.assertEqual(len({name for _, name, _ in runs}), len(runs))
        for role in ("client", "server"):
            self.assertEqual([case for kind, _, batch in runs if kind == role
                              for case in batch], expected)
        self.assertEqual(runs[0], ("client", "client", expected))
        self.assertTrue(all(len(batch) <= 64 for role, _, batch in runs if role == "server"))

    def test_small_selection_keeps_standard_report_name_and_order(self):
        expected = ["13.4.13", "1.1.1", "6.4.1"]
        self.assertEqual(list(case_batches(["server"], expected)),
                         [("server", "server", expected)])

    def test_exact_batch_boundary(self):
        expected = list(range(64))
        self.assertEqual(list(case_batches(["server"], expected)),
                         [("server", "server", expected)])
