"""Observed-side helper: run the implementation under test and compare.

The comparison never feeds a tolerance back into the truth and never reads an
observed value to reconstruct an input.  Each functional is compared with a
tolerance that was frozen before the run; a missing implementation is
reported as ``pending_implementation`` with the reason.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from . import driver
from .vcommon import (
    Case,
    comparison_payload,
    content_hash,
    environment_identity,
    read_json,
    sha256_file,
    source_identity,
    write_json,
)


def run_native_driver(case: Case, binary_name: str, argv: list[str] | None = None
                      ) -> tuple[dict[str, Any], dict[str, Any]]:
    """Run the native in-memory API driver and store its raw response."""
    binary = driver.native_binary(binary_name)
    output_path = case.root / "observed" / f"native_{case.name.lower()}.json"
    arguments = list(argv or [])
    if "--output" not in arguments:
        arguments += ["--output", str(output_path)]
    run = driver.run_native(binary, arguments)
    payload = run.as_dict()
    payload["binary_identity"] = driver.binary_identity(binary)
    if run.status == "ok" and output_path.is_file():
        payload["response_file"] = str(output_path.relative_to(case.root.parent))
        payload["response_sha256"] = sha256_file(output_path)
        try:
            payload["response"] = read_json(output_path)
        except Exception:
            payload["response"] = None
    write_json(case.root / "observed" / f"native_{case.name.lower()}_run.json", payload)
    return payload, payload.get("response") or {}


def run_contract_driver(case: Case) -> tuple[dict[str, Any], dict[str, Any]]:
    """Run the acsd-contract process driver on the frozen request."""
    request_path = case.root / "request.json"
    response_path = case.root / "observed" / f"contract_{case.name.lower()}.json"
    run = driver.run_contract(request_path, response_path)
    payload = run.as_dict()
    payload["binary_identity"] = driver.binary_identity(driver.contract_binary())
    write_json(case.root / "observed" / f"contract_{case.name.lower()}_run.json", payload)
    return payload, payload.get("response") or {}


def observed_values(response: dict[str, Any]) -> dict[str, float]:
    """Extract the numeric observables from a native driver response."""
    values: dict[str, float] = {}
    results = response.get("results") if isinstance(response, dict) else None
    if isinstance(results, list):
        for entry in results:
            if isinstance(entry, dict):
                for key, value in (entry.get("values") or {}).items():
                    if isinstance(value, (int, float)):
                        values[key] = float(value)
    return values


def observed_texts(response: dict[str, Any]) -> dict[str, str]:
    texts: dict[str, str] = {}
    results = response.get("results") if isinstance(response, dict) else None
    if isinstance(results, list):
        for entry in results:
            if isinstance(entry, dict):
                for key, value in (entry.get("text") or {}).items():
                    texts[key] = str(value)
    return texts


def compare(case: Case,
            functionals: list[dict[str, Any]],
            observed: dict[str, float],
            observed_text: dict[str, str],
            observed_status: str,
            observed_reason: str = "",
            notes: str | None = None) -> dict[str, Any]:
    """Compare one frozen functional list against the observed values.

    ``functionals`` entries look like::

        {"name": ..., "truth": float, "tolerance": float, "unit": ..., "kind": "value"}
        {"name": ..., "expect": "absent"|"present"|"code:<name>", ...}
    """
    rows: list[dict[str, Any]] = []
    failures: list[dict[str, Any]] = []
    overall = "pass"

    for functional in functionals:
        name = functional["name"]
        kind = functional.get("kind", "value")
        row: dict[str, Any] = {"name": name, "kind": kind, "unit": functional.get("unit", "")}
        if observed_status != "ok":
            row.update({"status": "pending_implementation",
                        "truth": functional.get("truth"),
                        "observed": None,
                        "reason": observed_reason or "the public entry point is not available yet"})
            rows.append(row)
            overall = "pending_implementation" if overall == "pass" else overall
            continue

        if kind == "value":
            truth = float(functional["truth"])
            tolerance = float(functional["tolerance"])
            if name not in observed:
                row.update({"status": "pending_implementation", "truth": truth, "observed": None,
                            "reason": "the driver did not report this functional"})
                failures.append({"functional": name, "scope": "driver_output",
                                 "expected": truth, "actual": None,
                                 "reason": "the functional is missing from the driver response"})
                overall = "fail" if overall != "pending_implementation" else overall
            else:
                actual = float(observed[name])
                difference = abs(actual - truth)
                inside = difference <= tolerance
                row.update({"status": "pass" if inside else "fail", "truth": truth,
                            "observed": actual, "absolute_difference": difference,
                            "tolerance": tolerance,
                            "relative_difference": difference / abs(truth) if truth else None})
                if not inside:
                    failures.append({"functional": name, "scope": "public_api",
                                     "expected": truth, "actual": actual,
                                     "tolerance": tolerance,
                                     "reason": "the observed value is outside the frozen tolerance"})
                    overall = "fail"
        elif kind == "code":
            expected_codes = functional.get("codes", [])
            actual_code = observed_text.get(functional.get("text_key", name))
            ok_codes = str(actual_code) in [str(code) for code in expected_codes]
            row.update({"status": "pass" if ok_codes else "fail",
                        "expected_codes": expected_codes, "observed_code": actual_code})
            if not ok_codes:
                failures.append({"functional": name, "scope": "public_api",
                                 "expected": expected_codes, "actual": actual_code,
                                 "reason": "the implementation did not return a named failure code"})
                overall = "fail"
        elif kind == "text":
            needle = functional.get("contains")
            haystack = observed_text.get(functional.get("text_key", name), "")
            found = needle in haystack if needle else bool(haystack)
            row.update({"status": "pass" if found else "fail",
                        "expected_contains": needle, "observed_text": haystack})
            if not found:
                failures.append({"functional": name, "scope": "public_api",
                                 "expected": needle, "actual": haystack,
                                 "reason": "the expected diagnostic text was not reported"})
                overall = "fail"
        elif kind == "differ":
            # the two named observables must differ by more than the tolerance
            tolerance = float(functional["tolerance"])
            actual = observed.get(functional["first"])
            other = observed.get(functional["second"])
            if actual is None or other is None:
                row.update({"status": "pending_implementation",
                            "reason": "one of the two observables is missing"})
                failures.append({"functional": name, "scope": "public_api",
                                 "expected": f"|{functional['first']} - {functional['second']}| > {tolerance}",
                                 "actual": None,
                                 "reason": "the driver did not report both observables"})
                overall = "fail"
            else:
                difference = abs(actual - other)
                inside = difference > tolerance
                row.update({"status": "pass" if inside else "fail",
                            "difference": difference, "tolerance": tolerance,
                            "expected": f"difference > {tolerance}"})
                if not inside:
                    failures.append({"functional": name, "scope": "public_api",
                                     "expected": f"difference > {tolerance}",
                                     "actual": difference,
                                     "reason": "the false-independence shortcut was not separated"})
                    overall = "fail"
        rows.append(row)

    payload = comparison_payload(case.name.upper(), overall, rows, failures, notes)
    case.write_comparison(payload)
    return payload


def write_manifest(case: Case, extra: dict[str, Any] | None = None) -> None:
    manifest = {
        "case": case.name.upper(),
        "environment": environment_identity(),
        "code_identity": source_identity(
            [p for p in (case.root / f"truth_{case.name.lower()}.py",) if p.is_file()]),
        "requests": {"request.json": sha256_file(case.root / "request.json")
                     if (case.root / "request.json").is_file() else ""},
    }
    if extra:
        manifest.update(extra)
    case.write_manifest(manifest)
