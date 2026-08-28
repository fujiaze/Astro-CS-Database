#!/usr/bin/env python3
"""Validate that the V4 control package itself is complete and internally strict."""

from __future__ import annotations

import csv
import hashlib
import pathlib
import re
import sys

VALID_STATES = {"NOT_STARTED", "IN_PROGRESS", "PASS", "FAIL", "BLOCKED"}
REQUIRED = {
    "00_READ_FIRST.md",
    "01_WORKFLOW_AND_GATES.md",
    "02_TASK_LEDGER.csv",
    "03_TASK_SPECIFICATIONS.md",
    "04_CHECKPOINT_CHECKLISTS.md",
    "05_NUMERICAL_AND_PERFORMANCE_GATES.md",
    "06_GIT_MAIN_ONLY.md",
    "07_AUDIT_PACKAGE_SPEC.md",
    "08_V2_INHERITED_FINDINGS.md",
    "templates/CHECKPOINT_RESULTS.csv",
    "templates/COMMITS.csv",
    "templates/FINDINGS.csv",
    "templates/BUILD_RESULTS.csv",
    "templates/TEST_RESULTS.csv",
    "templates/PERF_RESULTS.csv",
    "templates/LARGE_ARTIFACT_MANIFEST.csv",
    "templates/TRACEABILITY.csv",
    "templates/SUMMARY.schema.json",
    "scripts/validate_audit_package.py",
    "scripts/package_audit.py",
}


def fail(errors: list[str], message: str) -> None:
    errors.append(message)


def main() -> int:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    errors: list[str] = []
    for rel in sorted(REQUIRED):
        path = root / rel
        if not path.is_file():
            fail(errors, f"missing required file: {rel}")
        elif path.stat().st_size == 0:
            fail(errors, f"empty required file: {rel}")

    ledger = root / "02_TASK_LEDGER.csv"
    if ledger.is_file():
        with ledger.open(newline="", encoding="utf-8-sig") as handle:
            rows = list(csv.DictReader(handle))
        expected_fields = {
            "task_id", "gate", "depends_on", "scope", "required_commit",
            "required_push", "checkpoint", "status",
        }
        if not rows:
            fail(errors, "task ledger has no rows")
        elif set(rows[0]) != expected_fields:
            fail(errors, f"task ledger fields differ: {set(rows[0])}")
        ids = [row["task_id"] for row in rows]
        if len(ids) != len(set(ids)):
            fail(errors, "duplicate task_id")
        known = set(ids) | {f"C{i}" for i in range(10)}
        if len(ids) != 98:
            fail(errors, f"task ledger must have exactly 98 rows, got {len(ids)}")
        for row in rows:
            if row["status"] not in VALID_STATES:
                fail(errors, f"invalid state {row['task_id']}: {row['status']}")
            if row["required_commit"] not in {"yes", "no"}:
                fail(errors, f"invalid required_commit {row['task_id']}")
            if row["required_push"] not in {"yes", "no"}:
                fail(errors, f"invalid required_push {row['task_id']}")
            if row["depends_on"] and row["depends_on"] not in known:
                fail(errors, f"unknown dependency {row['task_id']}: {row['depends_on']}")
            if not re.fullmatch(r"C[0-9]", row["checkpoint"]):
                fail(errors, f"invalid checkpoint {row['task_id']}: {row['checkpoint']}")

    if errors:
        print("CONTROL_FAIL")
        for error in errors:
            print(f"- {error}")
        return 1
    digest = hashlib.sha256()
    for path in sorted(p for p in root.rglob("*") if p.is_file()):
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(path.read_bytes())
    print(f"CONTROL_PASS files={sum(1 for p in root.rglob('*') if p.is_file())} sha256={digest.hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
