#!/usr/bin/env python3
from __future__ import annotations

import csv
import hashlib
import json
import pathlib
import re
import sys

MAX_FILE = 5 * 1024 * 1024
MAX_TOTAL = 25 * 1024 * 1024
REQUIRED = {
    "00_READ_FIRST.md", "FINAL_REPORT.md", "SUMMARY.json", "TASK_LEDGER.csv",
    "COMMITS.csv", "FINDINGS.csv", "BUILD_RESULTS.csv", "TEST_RESULTS.csv",
    "CPU_PROFILE_RESULTS.csv", "RESOURCE_RESULTS.csv", "LARGE_ARTIFACTS.csv",
    "TRACEABILITY.csv", "REVIEW_CAPSULE_INDEX.csv", "SCIENCE_CLAIMS.csv",
    "RELEASE_ARTIFACTS.csv", "CHECKPOINTS.csv", "MANIFEST.json", "SHA256SUMS",
}
FORBIDDEN_DIRS = {".git", "build", "builds", "cache", "tmp", "temp", "testdata", "hips"}
FORBIDDEN_SUFFIXES = {".fits", ".fit", ".fts", ".xisf", ".hiss", ".o", ".obj", ".a", ".lib", ".so", ".dll", ".exe", ".pdb", ".zip", ".tar", ".tgz", ".zst"}


def table(path: pathlib.Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as handle:
        return list(csv.DictReader(handle))


def counts(rows: list[dict[str, str]], key: str) -> dict[str, int]:
    result: dict[str, int] = {}
    for row in rows:
        value = row.get(key, "")
        result[value] = result.get(value, 0) + 1
    return dict(sorted(result.items()))


def main() -> int:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    errors: list[str] = []
    files = sorted(path for path in root.rglob("*") if path.is_file())
    rels = {path.relative_to(root).as_posix() for path in files}
    errors.extend(f"missing: {name}" for name in sorted(REQUIRED - rels))
    total = 0
    for path in files:
        relative = path.relative_to(root)
        size = path.stat().st_size
        total += size
        if size == 0:
            errors.append(f"empty: {relative}")
        if size > MAX_FILE:
            errors.append(f">5MiB: {relative}")
        if any(part.lower() in FORBIDDEN_DIRS for part in relative.parts):
            errors.append(f"forbidden dir: {relative}")
        if path.suffix.lower() in FORBIDDEN_SUFFIXES:
            errors.append(f"forbidden type: {relative}")
    if total > MAX_TOTAL:
        errors.append(f">25MiB total: {total}")

    names = ["TASK_LEDGER.csv", "COMMITS.csv", "FINDINGS.csv", "BUILD_RESULTS.csv", "TEST_RESULTS.csv", "CPU_PROFILE_RESULTS.csv", "RESOURCE_RESULTS.csv", "TRACEABILITY.csv", "REVIEW_CAPSULE_INDEX.csv", "RELEASE_ARTIFACTS.csv", "CHECKPOINTS.csv"]
    tables: dict[str, list[dict[str, str]]] = {}
    for name in names:
        path = root / name
        if path.is_file():
            try:
                tables[name] = table(path)
            except Exception as exc:
                errors.append(f"csv parse {name}: {exc}")

    ledger = tables.get("TASK_LEDGER.csv", [])
    incomplete = [row.get("task_id", "") for row in ledger if row.get("status") != "PASS"]
    if incomplete:
        errors.append(f"non-PASS tasks: {incomplete[:20]}")
    commits = tables.get("COMMITS.csv", [])
    for row in commits:
        if row.get("branch") != "main" or row.get("push_status") != "PASS" or row.get("status") != "PASS":
            errors.append(f"invalid commit row: {row.get('task_id')}")
    for row in tables.get("FINDINGS.csv", []):
        if row.get("severity") in {"P0", "P1"} and row.get("status") != "CLOSED":
            errors.append(f"open {row.get('severity')}: {row.get('finding_id')}")
    for row in tables.get("BUILD_RESULTS.csv", []):
        if row.get("status") != "PASS" or row.get("exit_code") != "0" or row.get("ignored_errors") not in {"0", ""}:
            errors.append(f"invalid build: {row.get('build_id')}")
    for row in tables.get("TEST_RESULTS.csv", []):
        if row.get("status") != "PASS":
            errors.append(f"failed test: {row.get('test_id')}")
    for row in tables.get("RESOURCE_RESULTS.csv", []):
        if row.get("verdict") != "PASS":
            errors.append(f"resource failure: {row.get('run_id')}/{row.get('stage')}")
    for row in tables.get("CHECKPOINTS.csv", []):
        if row.get("status") != "PASS":
            errors.append(f"checkpoint not PASS: {row.get('checkpoint_id')}")
    artifacts = tables.get("RELEASE_ARTIFACTS.csv", [])
    platforms = {row.get("platform") for row in artifacts if row.get("status") == "PASS"}
    if not {"Linux", "Windows"}.issubset(platforms):
        errors.append("missing PASS Linux/Windows alpha artifacts")
    for row in artifacts:
        if row.get("status") == "PASS" and not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+-alpha\.[0-9]+", row.get("version", "")):
            errors.append(f"non-alpha artifact: {row.get('artifact_id')}")

    summary_path = root / "SUMMARY.json"
    if summary_path.is_file():
        try:
            summary = json.loads(summary_path.read_text(encoding="utf-8"))
            if summary.get("verdict") != "AWAITING_EXTERNAL_RELEASE_REVIEW":
                errors.append("invalid final verdict")
            if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+-alpha\.[0-9]+", summary.get("version", "")):
                errors.append("SUMMARY version is not alpha")
            if not summary.get("windows_32r_run_id"):
                errors.append("missing windows_32r_run_id")
            expected = {
                "task_counts": counts(ledger, "status"),
                "finding_counts": counts(tables.get("FINDINGS.csv", []), "severity"),
                "build_counts": counts(tables.get("BUILD_RESULTS.csv", []), "status"),
                "test_counts": counts(tables.get("TEST_RESULTS.csv", []), "status"),
                "resource_counts": counts(tables.get("RESOURCE_RESULTS.csv", []), "verdict"),
            }
            for key, value in expected.items():
                if summary.get(key) != value:
                    errors.append(f"SUMMARY mismatch {key}: {summary.get(key)} != {value}")
        except Exception as exc:
            errors.append(f"SUMMARY parse: {exc}")

    sums = root / "SHA256SUMS"
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
        for relative, digest in listed.items():
            path = root / relative
            if path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() != digest:
                errors.append(f"hash mismatch: {relative}")

    if errors:
        print("FINAL_PACKAGE_FAIL")
        for error in errors:
            print(f"- {error}")
        return 1
    print(f"FINAL_PACKAGE_PASS files={len(files)} bytes={total}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
