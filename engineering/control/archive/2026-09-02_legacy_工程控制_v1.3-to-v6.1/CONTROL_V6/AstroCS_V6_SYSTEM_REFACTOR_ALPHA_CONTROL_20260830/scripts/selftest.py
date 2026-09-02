#!/usr/bin/env python3
"""Negative and positive smoke tests for control-package validators."""

from __future__ import annotations

import copy
import csv
import hashlib
import json
import pathlib
import shutil
import tempfile

from package_audit import ALLOWED_ROOT_FILES, collect
from validate_audit import validate as validate_audit
from validate_task_graph import LedgerError, load_ledger, validate_rows


def expect_error(fn, contains: str) -> None:
    try:
        fn()
    except (LedgerError, ValueError) as exc:
        if contains not in str(exc):
            raise AssertionError(f"expected {contains!r}, got {exc!r}") from exc
    else:
        raise AssertionError(f"expected failure containing {contains!r}")


def test_ledger(root: pathlib.Path) -> None:
    rows = load_ledger(root / "05_TASK_LEDGER.csv")
    result = validate_rows(rows)
    assert result["task_count"] >= 80
    assert result["topological_order"][-1] == "REL-004"

    unknown = copy.deepcopy(rows)
    unknown[1]["depends_on"] = "NOPE-999"
    expect_error(lambda: validate_rows(unknown), "unknown dependency")

    cycle = copy.deepcopy(rows)
    cycle[0]["depends_on"] = cycle[-1]["task_id"]
    expect_error(lambda: validate_rows(cycle), "dependency")

    bad_state = copy.deepcopy(rows)
    bad_state[1]["status"] = "PASS"
    expect_error(lambda: validate_rows(bad_state), "non-PASS dependencies")

    duplicate = copy.deepcopy(rows)
    duplicate[1]["task_id"] = duplicate[0]["task_id"]
    expect_error(lambda: validate_rows(duplicate), "duplicate task_id")


def test_packaging_whitelist() -> None:
    with tempfile.TemporaryDirectory(prefix="astrocs-control-test-") as td:
        root = pathlib.Path(td)
        for name in ALLOWED_ROOT_FILES:
            (root / name).write_text("x\n", encoding="utf-8")
        review = root / "docs/review/REVIEW.md"
        review.parent.mkdir(parents=True)
        review.write_text("ok\n", encoding="utf-8")
        assert collect(root)
        forbidden = root / "testdata/frame.fits"
        forbidden.parent.mkdir()
        forbidden.write_bytes(b"x")
        expect_error(lambda: collect(root), "not whitelisted")


def file_sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def test_minimal_not_ready_audit(control_root: pathlib.Path) -> None:
    with tempfile.TemporaryDirectory(prefix="astrocs-audit-validator-") as td:
        root = pathlib.Path(td)
        ledger = root / "TASK_LEDGER.csv"
        shutil.copyfile(control_root / "05_TASK_LEDGER.csv", ledger)
        rows = load_ledger(ledger)
        ledger_result = validate_rows(rows)
        source = "1" * 40
        summary = {
            "schema": "astrocs.audit-summary/v1",
            "version": "0.10.0-alpha.1",
            "verdict": "NOT_READY",
            "source_commit": source,
            "origin_main_commit": source,
            "control_sha256": "2" * 64,
            "ledger_sha256": file_sha(ledger),
            "task_counts": ledger_result["task_counts"],
            "gate_status": ledger_result["gate_status"],
            "science_changed": False,
            "blockers": ["fixture"],
            "not_verified": ["fixture"],
            "linux": {},
            "windows": {},
            "artifacts": [],
            "owner_review": "PENDING",
        }
        (root / "SUMMARY.json").write_text(json.dumps(summary), encoding="utf-8")
        (root / "00_README.md").write_text("fixture\n", encoding="utf-8")
        (root / "SOURCE_IDENTITY.json").write_text(json.dumps({"source_commit": source}), encoding="utf-8")
        csv_files = {
            "COMMITS.csv": ["task_id", "commit"],
            "FINDINGS.csv": ["finding_id", "severity", "status"],
            "TEST_SUMMARY.csv": ["test_id", "result"],
            "RESOURCE_SUMMARY.csv": ["module_id", "gate_status"],
            "TRACEABILITY_MATRIX.csv": ["contract_id", "source"],
            "LARGE_ARTIFACT_MANIFEST.csv": ["artifact_id", "sha256"],
            "PREVIEW_MANIFEST.csv": ["preview_id", "sha256"],
        }
        for name, header in csv_files.items():
            with (root / name).open("w", encoding="utf-8", newline="") as f:
                csv.writer(f).writerow(header)
        review = root / "docs/review/REVIEW.md"
        review.parent.mkdir(parents=True)
        review.write_text("fixture\n", encoding="utf-8")
        entries = []
        for path in sorted(root.rglob("*")):
            if path.is_file():
                entries.append({"path": path.relative_to(root).as_posix(), "size": path.stat().st_size, "sha256": file_sha(path)})
        (root / "MANIFEST.json").write_text(json.dumps({"schema": "astrocs.audit-manifest/v1", "files": entries}), encoding="utf-8")
        (root / "SHA256SUMS").write_text("".join(f"{e['sha256']}  {e['path']}\n" for e in entries), encoding="utf-8")
        result = validate_audit(root)
        assert result["verdict"] == "NOT_READY"


def main() -> int:
    root = pathlib.Path(__file__).resolve().parent.parent
    test_ledger(root)
    test_packaging_whitelist()
    test_minimal_not_ready_audit(root)
    print("SELFTEST_PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
