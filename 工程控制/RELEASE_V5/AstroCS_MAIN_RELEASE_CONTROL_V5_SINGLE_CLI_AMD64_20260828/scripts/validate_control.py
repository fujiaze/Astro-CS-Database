#!/usr/bin/env python3
from __future__ import annotations

import csv
import hashlib
import json
import pathlib
import re
import sys

VALID_STATES = {"NOT_STARTED", "IN_PROGRESS", "PASS", "FAIL", "BLOCKED", "REVIEW_PENDING"}
REQUIRED = {
    "00_READ_FIRST.md", "01_PRODUCT_ARCHITECTURE.md", "02_TASK_LEDGER.csv",
    "03_TASK_DETAILS.md", "04_CLI_COMMAND_AND_PROTOCOL_CONTRACT.md",
    "05_CPU_BACKEND_ABI_AND_PACKAGING.md", "06_BENCHMARK_AND_PROFILE_SPEC.md",
    "07_RESOURCE_MONITOR_AND_UTILIZATION_GATE.md",
    "08_SCIENCE_SYNTHETIC_AND_EXTERNAL_REVIEW.md",
    "09_LINUX_WINDOWS_BUILD_RELEASE.md", "10_GIT_REVIEW_CAPSULE_AUDIT_PACKAGE.md",
    "11_AGENTS_MD_REQUIRED_BLOCK.md", "12_V3_V4_MIGRATION.md",
    "13_ALPHA_VERSION_AND_PHASE3.md", "14_V4_COVERAGE_MATRIX.md",
    "15_CONTINUOUS_CHECKPOINTS.md", "PACKAGE_MANIFEST.md", "SHA256SUMS", "START_PROMPT.txt",
    "schemas/cpu_profile.schema.json", "schemas/cli_event.schema.json",
    "scripts/package_final.py", "scripts/validate_final_package.py",
    "templates/COMMITS.csv", "templates/FINDINGS.csv", "templates/BUILD_RESULTS.csv",
    "templates/TEST_RESULTS.csv", "templates/CPU_PROFILE_RESULTS.csv",
    "templates/RESOURCE_RESULTS.csv", "templates/LARGE_ARTIFACTS.csv",
    "templates/TRACEABILITY.csv", "templates/REVIEW_CAPSULE_INDEX.csv",
    "templates/SCIENCE_CLAIMS.csv", "templates/RELEASE_ARTIFACTS.csv",
    "templates/CHECKPOINTS.csv",
    "templates/SUMMARY.json",
}
EXPECTED_FIELDS = ["task_id", "phase", "depends_on", "scope", "required_commit", "required_push", "preferred_host", "status"]
MANDATORY_PREFIXES = {
    "SCI-", "ALG-", "ARCH-", "API-", "CLI-", "P3-", "ABI-", "ISA-",
    "BENCH-", "MON-", "PAR-", "SYN-", "DOCCHK-", "LNX-", "WIN-", "REL-",
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

    rows: list[dict[str, str]] = []
    ledger = root / "02_TASK_LEDGER.csv"
    if ledger.is_file():
        try:
            with ledger.open(newline="", encoding="utf-8-sig") as handle:
                reader = csv.DictReader(handle)
                if reader.fieldnames != EXPECTED_FIELDS:
                    errors.append(f"ledger fields: {reader.fieldnames} != {EXPECTED_FIELDS}")
                rows = list(reader)
        except Exception as exc:
            errors.append(f"ledger parse: {exc}")
    if len(rows) < 85:
        errors.append(f"task count too small: {len(rows)} < 85")

    ids = [row.get("task_id", "") for row in rows]
    if len(ids) != len(set(ids)):
        errors.append("duplicate task_id")
    known = set(ids)
    seen: set[str] = set()
    for row in rows:
        task = row.get("task_id", "")
        if row.get("status") not in VALID_STATES:
            errors.append(f"invalid status: {task}={row.get('status')}")
        if row.get("required_commit") not in {"yes", "no"} or row.get("required_push") not in {"yes", "no"}:
            errors.append(f"invalid commit/push: {task}")
        if row.get("required_push") == "yes" and row.get("required_commit") != "yes":
            errors.append(f"push without commit: {task}")
        for dep in filter(None, row.get("depends_on", "").split("|")):
            if dep not in known:
                errors.append(f"unknown dependency: {task}->{dep}")
            elif dep not in seen:
                errors.append(f"forward dependency: {task}->{dep}")
        seen.add(task)
    for prefix in MANDATORY_PREFIXES:
        if not any(item.startswith(prefix) for item in ids):
            errors.append(f"missing task family: {prefix}")

    for rel in ["schemas/cpu_profile.schema.json", "schemas/cli_event.schema.json", "templates/SUMMARY.json"]:
        path = root / rel
        if path.is_file():
            try:
                json.loads(path.read_text(encoding="utf-8"))
            except Exception as exc:
                errors.append(f"json parse {rel}: {exc}")

    docs = "\n".join(path.read_text(encoding="utf-8", errors="replace") for path in root.glob("*.md"))
    prompt = (root / "START_PROMPT.txt").read_text(encoding="utf-8", errors="replace") if (root / "START_PROMPT.txt").is_file() else ""
    required_phrases = [
        "alpha", "HiPS", "FITS", "Phase3", "baseline", "AVX-512", "cpu_profile.json",
        "AWAITING_EXTERNAL_RELEASE_REVIEW", "32R", "REVIEW_PENDING", "main",
    ]
    for phrase in required_phrases:
        if phrase not in docs + prompt:
            errors.append(f"missing control concept: {phrase}")
    forbidden_ledger = ["ACR-001", "RUN-003", "RUN-004", "CP0"]
    ledger_text = ledger.read_text(encoding="utf-8", errors="replace") if ledger.is_file() else ""
    for token in forbidden_ledger:
        if token in ledger_text:
            errors.append(f"obsolete task/gate in ledger: {token}")
    if re.search(r"(?i)git\s+(?:checkout\s+-b|switch\s+-c)", docs + prompt):
        errors.append("branch creation command present")

    sums = root / "SHA256SUMS"
    files = sorted(path for path in root.rglob("*") if path.is_file())
    rels = {path.relative_to(root).as_posix() for path in files}
    if sums.is_file():
        listed: dict[str, str] = {}
        for line in sums.read_text(encoding="utf-8").splitlines():
            match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
            if not match:
                errors.append(f"bad SHA256SUMS line: {line[:80]}")
                continue
            listed[match.group(2)] = match.group(1)
        if set(listed) != rels - {"SHA256SUMS"}:
            errors.append("SHA256SUMS file set mismatch")
        for relative, digest_value in listed.items():
            path = root / relative
            if path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() != digest_value:
                errors.append(f"hash mismatch: {relative}")

    if errors:
        print("CONTROL_FAIL")
        for item in errors:
            print(f"- {item}")
        return 1
    digest = hashlib.sha256()
    for path in files:
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(b"\0")
        digest.update(path.read_bytes())
    print(f"CONTROL_PASS tasks={len(rows)} files={len(files)} sha256={digest.hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
