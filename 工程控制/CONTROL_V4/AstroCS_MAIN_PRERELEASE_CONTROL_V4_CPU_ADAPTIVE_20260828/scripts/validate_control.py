#!/usr/bin/env python3
from __future__ import annotations

import csv
import hashlib
import pathlib
import re
import sys

VALID_STATES = {"NOT_STARTED", "IN_PROGRESS", "PASS", "FAIL", "BLOCKED", "DEFERRED", "REVIEW_PENDING"}
REQUIRED = {
    "00_READ_FIRST.md", "01_SCOPE_AND_WORKFLOW.md", "02_TASK_LEDGER.csv",
    "03_TASK_DETAILS.md", "04_CPU_AUTOTUNE_SPEC.md", "05_RESOURCE_MONITOR_SPEC.md",
    "06_SYNTHETIC_VALIDATION_MATRIX.md", "07_DOC_CODE_CONSISTENCY.md",
    "08_LINUX_WINDOWS_POLICY.md", "09_GIT_AND_AUDIT_PACKAGE.md",
    "10_AGENTS_MD_REQUIRED_BLOCK.md", "11_V3_INHERITANCE_AND_INVALIDATION.md",
    "12_REVIEW_CAPSULE_AND_EXTERNAL_AUDIT.md",
    "templates/COMMITS.csv", "templates/FINDINGS.csv", "templates/BUILD_RESULTS.csv",
    "templates/TEST_RESULTS.csv", "templates/CPU_PROFILE_RESULTS.csv",
    "templates/RESOURCE_RESULTS.csv", "templates/LARGE_ARTIFACTS.csv",
    "templates/TRACEABILITY.csv", "scripts/validate_final_package.py",
    "templates/REVIEW_CAPSULE_INDEX.csv", "templates/SCIENCE_CLAIMS.csv",
    "scripts/package_final.py",
}


def main() -> int:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    errors: list[str] = []
    for rel in sorted(REQUIRED):
        path = root / rel
        if not path.is_file():
            errors.append(f"missing: {rel}")
        elif path.stat().st_size == 0:
            errors.append(f"empty: {rel}")

    ledger = root / "02_TASK_LEDGER.csv"
    if ledger.is_file():
        with ledger.open(newline="", encoding="utf-8-sig") as handle:
            rows = list(csv.DictReader(handle))
        fields = {"task_id", "phase", "depends_on", "scope", "required_commit", "required_push", "preferred_host", "status"}
        if not rows or set(rows[0]) != fields:
            errors.append("invalid ledger fields or no rows")
        ids = [row["task_id"] for row in rows]
        if len(ids) != len(set(ids)):
            errors.append("duplicate task_id")
        known = set(ids)
        seen: set[str] = set()
        for row in rows:
            if row["status"] not in VALID_STATES:
                errors.append(f"invalid state: {row['task_id']}={row['status']}")
            if row["required_commit"] not in {"yes", "no"} or row["required_push"] not in {"yes", "no"}:
                errors.append(f"invalid commit/push flag: {row['task_id']}")
            deps = [x for x in row["depends_on"].split("|") if x]
            for dep in deps:
                if dep not in known:
                    errors.append(f"unknown dependency: {row['task_id']}->{dep}")
                if dep not in seen:
                    errors.append(f"forward dependency: {row['task_id']}->{dep}")
            seen.add(row["task_id"])

    text = "\n".join(p.read_text(encoding="utf-8", errors="replace") for p in root.glob("*.md"))
    forbidden = ["RUN-003", "RUN-004", "RUN-005", "RUN-006", "ACR-001", "ACR-002", "ACR-003"]
    # Mentions in the inheritance/cancellation docs are permitted; ledger is authoritative.
    if ledger.is_file():
        ledger_text = ledger.read_text(encoding="utf-8")
        for token in forbidden:
            if token in ledger_text:
                errors.append(f"cancelled task present in ledger: {token}")

    if errors:
        print("CONTROL_FAIL")
        for item in errors:
            print(f"- {item}")
        return 1
    digest = hashlib.sha256()
    files = sorted(p for p in root.rglob("*") if p.is_file())
    for path in files:
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(path.read_bytes())
    print(f"CONTROL_PASS tasks={len(rows)} files={len(files)} sha256={digest.hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
