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
VALID_STATES = {"NOT_STARTED", "IN_PROGRESS", "PASS", "FAIL", "BLOCKED", "DEFERRED", "REVIEW_PENDING"}
REQUIRED = {
    "00_READ_FIRST.md", "SUMMARY.json", "TASK_LEDGER.csv", "COMMITS.csv",
    "FINDINGS.csv", "BUILD_RESULTS.csv", "TEST_RESULTS.csv",
    "CPU_PROFILE_RESULTS.csv", "RESOURCE_RESULTS.csv", "TRACEABILITY.csv",
    "REVIEW_CAPSULE_INDEX.csv", "SCIENCE_CLAIMS.csv",
    "LARGE_ARTIFACTS.csv", "MANIFEST.json", "SHA256SUMS",
}
FORBIDDEN_DIRS = {".git", "build", "builds", "cache", "temp", "tmp", "hips"}
FORBIDDEN_SUFFIXES = {
    ".fits", ".fit", ".fts", ".xisf", ".hiss", ".o", ".obj", ".a", ".lib",
    ".so", ".dll", ".exe", ".pdb", ".bundle", ".tar", ".tgz",
}


def read_csv(path: pathlib.Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as handle:
        return list(csv.DictReader(handle))


def counts(rows: list[dict[str, str]], column: str) -> dict[str, int]:
    result: dict[str, int] = {}
    for row in rows:
        value = row.get(column, "")
        result[value] = result.get(value, 0) + 1
    return dict(sorted(result.items()))


def main() -> int:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    errors: list[str] = []
    files = sorted(p for p in root.rglob("*") if p.is_file())
    rels = {p.relative_to(root).as_posix() for p in files}
    for rel in REQUIRED - rels:
        errors.append(f"missing: {rel}")
    total = 0
    for path in files:
        rel = path.relative_to(root)
        size = path.stat().st_size
        total += size
        if size == 0:
            errors.append(f"empty: {rel}")
        if size > MAX_FILE:
            errors.append(f">5MiB: {rel}")
        if any(part.lower() in FORBIDDEN_DIRS for part in rel.parts):
            errors.append(f"forbidden dir: {rel}")
        if path.suffix.lower() in FORBIDDEN_SUFFIXES:
            errors.append(f"forbidden type: {rel}")
    if total > MAX_TOTAL:
        errors.append(f">25MiB total: {total}")

    tables: dict[str, list[dict[str, str]]] = {}
    for name in ["TASK_LEDGER.csv", "COMMITS.csv", "FINDINGS.csv", "BUILD_RESULTS.csv", "TEST_RESULTS.csv", "CPU_PROFILE_RESULTS.csv", "RESOURCE_RESULTS.csv", "TRACEABILITY.csv"]:
        path = root / name
        if path.is_file():
            try:
                tables[name] = read_csv(path)
            except Exception as exc:
                errors.append(f"csv parse {name}: {exc}")
    for name, rows in tables.items():
        for index, row in enumerate(rows, start=2):
            if "status" in row and row["status"] not in VALID_STATES:
                errors.append(f"{name}:{index} invalid status={row['status']}")

    summary_path = root / "SUMMARY.json"
    if summary_path.is_file():
        try:
            summary = json.loads(summary_path.read_text(encoding="utf-8"))
            if summary.get("verdict") != "AWAITING_EXTERNAL_RELEASE_REVIEW":
                errors.append("invalid final verdict")
            expected = {
                "task_counts": counts(tables.get("TASK_LEDGER.csv", []), "status"),
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

    sums_path = root / "SHA256SUMS"
    if sums_path.is_file():
        listed: dict[str, str] = {}
        for line in sums_path.read_text(encoding="utf-8").splitlines():
            match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
            if not match:
                errors.append("bad SHA256SUMS line")
                continue
            listed[match.group(2)] = match.group(1)
        if set(listed) != rels - {"SHA256SUMS"}:
            errors.append("SHA256SUMS set mismatch")
        for rel, digest in listed.items():
            path = root / rel
            if path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() != digest:
                errors.append(f"hash mismatch: {rel}")

    if errors:
        print("FINAL_PACKAGE_FAIL")
        for item in errors:
            print(f"- {item}")
        return 1
    print(f"FINAL_PACKAGE_PASS files={len(files)} bytes={total}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
