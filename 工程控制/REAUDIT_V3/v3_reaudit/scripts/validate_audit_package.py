#!/usr/bin/env python3
"""Strict whitelist validator for an AstroCS checkpoint/final audit directory."""

from __future__ import annotations

import csv
import hashlib
import json
import pathlib
import re
import sys

MAX_FILE = 5 * 1024 * 1024
MAX_TOTAL = 25 * 1024 * 1024
VALID_STATES = {"NOT_STARTED", "IN_PROGRESS", "PASS", "FAIL", "BLOCKED"}
REQUIRED = {
    "00_READ_FIRST.md", "SUMMARY.json", "TASK_LEDGER.csv",
    "CHECKPOINT_RESULTS.csv", "COMMITS.csv", "FINDINGS.csv",
    "BUILD_RESULTS.csv", "TEST_RESULTS.csv", "PERF_RESULTS.csv",
    "LARGE_ARTIFACT_MANIFEST.csv", "MANIFEST.json", "SHA256SUMS",
}
FORBIDDEN_DIRS = {".git", "build", "builds", "out", "cache", "temp", "tmp"}
FORBIDDEN_SUFFIXES = {
    ".fits", ".fit", ".fts", ".xisf", ".hiss", ".o", ".obj", ".a",
    ".lib", ".so", ".dll", ".exe", ".pdb", ".tar", ".tgz", ".bundle",
}
SECRET_PATTERNS = [
    re.compile(r"https?://[^\s/:]+:[^\s/@]+@"),
    re.compile(r"(?i)(token|password|secret)\s*[:=]\s*[^\s,]+"),
]


def rows(path: pathlib.Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as handle:
        return list(csv.DictReader(handle))


def count_by(items: list[dict[str, str]], column: str) -> dict[str, int]:
    result: dict[str, int] = {}
    for item in items:
        key = item.get(column, "")
        result[key] = result.get(key, 0) + 1
    return dict(sorted(result.items()))


def main() -> int:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    errors: list[str] = []
    files = sorted(p for p in root.rglob("*") if p.is_file())
    rels = {p.relative_to(root).as_posix() for p in files}
    for rel in sorted(REQUIRED - rels):
        errors.append(f"missing required file: {rel}")

    total = 0
    for path in files:
        rel = path.relative_to(root)
        size = path.stat().st_size
        total += size
        if size == 0:
            errors.append(f"empty file: {rel.as_posix()}")
        if size > MAX_FILE:
            errors.append(f"file >5MiB: {rel.as_posix()} ({size})")
        if any(part.lower() in FORBIDDEN_DIRS for part in rel.parts):
            errors.append(f"forbidden directory: {rel.as_posix()}")
        if path.suffix.lower() in FORBIDDEN_SUFFIXES:
            errors.append(f"forbidden suffix: {rel.as_posix()}")
        if path.suffix.lower() in {".md", ".txt", ".csv", ".json", ".log"} and size <= MAX_FILE:
            text = path.read_text(encoding="utf-8", errors="replace")
            for pattern in SECRET_PATTERNS:
                if pattern.search(text):
                    errors.append(f"possible secret in: {rel.as_posix()}")
                    break
    if total > MAX_TOTAL:
        errors.append(f"uncompressed evidence >25MiB: {total}")

    data: dict[str, list[dict[str, str]]] = {}
    for name in ["TASK_LEDGER.csv", "CHECKPOINT_RESULTS.csv", "COMMITS.csv", "FINDINGS.csv", "BUILD_RESULTS.csv", "TEST_RESULTS.csv"]:
        path = root / name
        if path.is_file():
            try:
                data[name] = rows(path)
            except Exception as exc:
                errors.append(f"cannot parse {name}: {exc}")

    for name, items in data.items():
        for index, item in enumerate(items, start=2):
            if "status" in item and item["status"] not in VALID_STATES:
                errors.append(f"{name}:{index} invalid status={item['status']}")

    summary_path = root / "SUMMARY.json"
    if summary_path.is_file():
        try:
            summary = json.loads(summary_path.read_text(encoding="utf-8"))
            if summary.get("verdict") not in {"AWAITING_EXTERNAL_REVIEW", "FAIL", "BLOCKED"}:
                errors.append("SUMMARY verdict lets agent self-accept")
            expected = {
                "task_counts": count_by(data.get("TASK_LEDGER.csv", []), "status"),
                "finding_counts": count_by(data.get("FINDINGS.csv", []), "severity"),
                "test_counts": count_by(data.get("TEST_RESULTS.csv", []), "status"),
                "build_counts": count_by(data.get("BUILD_RESULTS.csv", []), "status"),
            }
            for key, value in expected.items():
                if summary.get(key) != value:
                    errors.append(f"SUMMARY mismatch {key}: expected={value} actual={summary.get(key)}")
        except Exception as exc:
            errors.append(f"cannot parse SUMMARY.json: {exc}")

    # SHA256SUMS covers every regular file except itself.
    sums_path = root / "SHA256SUMS"
    if sums_path.is_file():
        expected_sums: dict[str, str] = {}
        for number, line in enumerate(sums_path.read_text(encoding="utf-8").splitlines(), start=1):
            match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
            if not match:
                errors.append(f"SHA256SUMS:{number} invalid line")
                continue
            digest, rel = match.groups()
            if rel in expected_sums:
                errors.append(f"SHA256SUMS duplicate: {rel}")
            expected_sums[rel] = digest
        actual_names = rels - {"SHA256SUMS"}
        if set(expected_sums) != actual_names:
            errors.append("SHA256SUMS file set mismatch")
        for rel, digest in expected_sums.items():
            path = root / rel
            if path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() != digest:
                errors.append(f"SHA256 mismatch: {rel}")

    # MANIFEST describes evidence files, excluding MANIFEST.json and SHA256SUMS.
    manifest_path = root / "MANIFEST.json"
    if manifest_path.is_file():
        try:
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest_files = manifest.get("files", [])
            actual_manifest_names = rels - {"MANIFEST.json", "SHA256SUMS"}
            listed_names = {item.get("path") for item in manifest_files}
            if listed_names != actual_manifest_names:
                errors.append("MANIFEST file set mismatch")
            if manifest.get("file_count") != len(actual_manifest_names):
                errors.append("MANIFEST file_count mismatch")
            actual_bytes = sum((root / rel).stat().st_size for rel in actual_manifest_names)
            if manifest.get("total_bytes") != actual_bytes:
                errors.append("MANIFEST total_bytes mismatch")
            for item in manifest_files:
                rel = item.get("path")
                path = root / rel if isinstance(rel, str) else None
                if not path or not path.is_file():
                    continue
                if item.get("size_bytes") != path.stat().st_size:
                    errors.append(f"MANIFEST size mismatch: {rel}")
                if item.get("sha256") != hashlib.sha256(path.read_bytes()).hexdigest():
                    errors.append(f"MANIFEST hash mismatch: {rel}")
        except Exception as exc:
            errors.append(f"cannot parse MANIFEST.json: {exc}")

    if errors:
        print("AUDIT_PACKAGE_FAIL")
        for error in errors:
            print(f"- {error}")
        return 1

    content_hash = hashlib.sha256()
    for path in files:
        content_hash.update(path.relative_to(root).as_posix().encode())
        content_hash.update(path.read_bytes())
    print(f"AUDIT_PACKAGE_PASS files={len(files)} bytes={total} sha256={content_hash.hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
