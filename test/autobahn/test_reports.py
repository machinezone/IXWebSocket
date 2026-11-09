"""Checks that missing results and failures cannot produce a green CI check."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    "check_autobahn", Path(__file__).resolve().parents[2] / "tools/check_autobahn.py")
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class ReportTest(unittest.TestCase):
    def report(self, behavior="OK", close="OK"):
        return {"ixwebsocket": {"1.1.1": {"behavior": behavior, "behaviorClose": close}}}

    def test_pass_and_informational(self):
        self.assertEqual(checker.check_report(self.report(), ["1.1.1"]), [])
        self.assertEqual(checker.check_report(
            self.report("INFORMATIONAL", "INFORMATIONAL"), ["1.1.1"]), [])

    def test_failures_in_both_columns(self):
        for status in ("FAILED", "NON-STRICT", "WRONG CODE", "UNCLEAN", "FAILED BY CLIENT",
                       "UNIMPLEMENTED", "unknown", None):
            with self.subTest(status=status):
                self.assertTrue(checker.check_report(self.report(status), ["1.1.1"]))
                self.assertTrue(checker.check_report(self.report(close=status), ["1.1.1"]))

    def test_missing_or_unexpected_cases(self):
        self.assertTrue(checker.check_report(self.report(), ["1.1.1", "1.1.2"]))
        self.assertTrue(checker.check_report(self.report(), ["1.1.2"]))
        self.assertTrue(checker.check_report({"ixwebsocket": {}}, ["1.1.1"]))

    def test_malformed_reports(self):
        for report in ({}, [], {"wrong-agent": {}}, {"ixwebsocket": []}):
            with self.subTest(report=report), self.assertRaises(ValueError):
                checker.check_report(report, ["1.1.1"])
        self.assertTrue(checker.check_report({"ixwebsocket": {"1.1.1": {}}}, ["1.1.1"]))
        self.assertTrue(checker.check_report({"ixwebsocket": {"1.1.1": None}}, ["1.1.1"]))

    def test_empty_or_duplicate_expected_cases(self):
        for expected in ([], ["1.1.1", "1.1.1"]):
            with self.assertRaises(ValueError):
                checker.check_report(self.report(), expected)


if __name__ == "__main__":
    unittest.main()
