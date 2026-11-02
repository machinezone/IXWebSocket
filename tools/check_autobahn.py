#!/usr/bin/env python3
"""Fail on noncompliant results or an incomplete Autobahn report."""
import argparse
import json
from pathlib import Path


PASS = {"OK", "INFORMATIONAL"}


def check_report(report, expected, agent="ixwebsocket"):
    if not isinstance(expected, list) or not expected or len(set(expected)) != len(expected):
        raise ValueError("Expected a nonempty list of unique case IDs")
    if not isinstance(report, dict) or set(report) != {agent}:
        raise ValueError("Report must contain exactly the expected agent: " + agent)
    cases = report[agent]
    if not isinstance(cases, dict):
        raise ValueError("Agent results must be an object")
    missing = sorted(set(expected) - set(cases))
    extra = sorted(set(cases) - set(expected))
    errors = []
    if missing:
        errors.append("Missing cases: " + ", ".join(missing))
    if extra:
        errors.append("Unexpected cases: " + ", ".join(extra))
    for case_id, result in sorted(cases.items()):
        if not isinstance(result, dict):
            errors.append(case_id + ": malformed result")
            continue
        for field in ("behavior", "behaviorClose"):
            status = result.get(field)
            if status not in PASS:
                errors.append("{}: {}={} ({})".format(
                    case_id, field, status, result.get("reportfile", "no detail report")))
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("expected", type=Path)
    args = parser.parse_args()
    try:
        report = json.loads(args.report.read_text())
        expected = json.loads(args.expected.read_text())
        errors = check_report(report, expected)
    except (OSError, ValueError, TypeError) as error:
        parser.exit(1, str(error) + "\n")
    for error in errors:
        print(error)
    print("{}: {} cases, {} failures".format(args.report, len(expected), len(errors)))
    return bool(errors)


if __name__ == "__main__":
    raise SystemExit(main())
