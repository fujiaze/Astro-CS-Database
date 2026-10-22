#!/usr/bin/env python3
"""Compares the real contract responses with the expected models.

The comparison is a post-processing step: the driver already produced the
responses from the request files alone.  No tolerance is relaxed here; the
numeric difference is reported as it is.
"""
from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path


def sha256_file(path: Path) -> str:
    return "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()


def close(a: float, b: float, tolerance: float = 1e-12) -> bool:
    return abs(a - b) <= tolerance * max(1.0, abs(a), abs(b))


def compare_u01(response: dict, expected: dict) -> dict:
    value = response["result"]["value"]["data"]
    truth = expected["data"]
    actual = value.get("value")
    report = {
        "case": "u01",
        "expected_value": truth.get("value"),
        "actual_value": actual,
        "difference": None if actual is None else actual - truth["value"],
        "exact": actual == truth["value"],
        "expected_state": truth.get("state"),
        "actual_state": value.get("state"),
        "expected_numeric_enclosure": truth.get("numeric_enclosure"),
        "actual_numeric_enclosure": value.get("numeric_enclosure"),
        "unit": value.get("unit"),
        "code": response["result"]["code"],
    }
    enclosure = value.get("numeric_enclosure")
    report["enclosure_contains_truth"] = bool(
        enclosure is not None
        and enclosure.get("lower", 0.0) <= truth["value"] <= enclosure.get("upper", 0.0)
    )
    report["pass"] = bool(
        report["exact"]
        and report["actual_state"] == report["expected_state"]
        and report["code"] == "ok"
        and report["enclosure_contains_truth"]
    )
    return report


def compare_external(response: dict, expected: dict) -> dict:
    value = response["result"]["value"]
    actual = value.get("values")
    truth = expected.get("values")
    report = {
        "case": "external-array",
        "expected_values": truth,
        "actual_values": actual,
        "difference": None
        if actual is None or truth is None
        else [a - b for a, b in zip(actual, truth)],
        "code": response["result"]["code"],
    }
    report["pass"] = bool(
        report["code"] == "ok"
        and actual is not None
        and len(actual) == len(truth)
        and all(close(a, b) for a, b in zip(actual, truth))
    )
    return report


def compare_to_fits(response: dict, expected: dict) -> dict:
    value = response["result"]["value"]["data"]
    truth = expected["data"]
    report = {
        "case": "to-fits",
        "expected": truth,
        "actual": value,
        "difference": {"x": value.get("x", 0.0) - truth["x"], "y": value.get("y", 0.0) - truth["y"]},
        "code": response["result"]["code"],
    }
    report["pass"] = bool(
        report["code"] == "ok"
        and close(value.get("x", 0.0), truth["x"])
        and close(value.get("y", 0.0), truth["y"])
    )
    return report


def main() -> int:
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "validation/apps/evidence")
    examples = Path(sys.argv[2] if len(sys.argv) > 2 else "examples")
    reports = []
    for name, comparator in (
        ("u01", compare_u01),
        ("external-array", compare_external),
        ("to-fits", compare_to_fits),
    ):
        response_path = out / f"{name}.response.json"
        expected_path = examples / f"{name}.expected-model.json"
        if not response_path.is_file():
            reports.append({"case": name, "pass": False, "reason": "no response file"})
            continue
        response = json.loads(response_path.read_text(encoding="utf-8"))
        expected = json.loads(expected_path.read_text(encoding="utf-8"))
        report = comparator(response, expected)
        report["response_sha256"] = sha256_file(response_path)
        report["request_sha256"] = sha256_file(examples / f"{name}.request.json")
        report["expected_model_sha256"] = sha256_file(expected_path)
        reports.append(report)

    summary = {
        "cases": reports,
        "all_pass": all(report.get("pass", False) for report in reports),
    }
    (out / "comparison.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    for report in reports:
        status = "PASS" if report.get("pass") else "FAIL"
        print(f"[{status}] {report.get('case')}: {json.dumps({k: v for k, v in report.items() if k not in {'response_sha256', 'request_sha256', 'expected_model_sha256'}}, sort_keys=True)}")
    return 0 if summary["all_pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
