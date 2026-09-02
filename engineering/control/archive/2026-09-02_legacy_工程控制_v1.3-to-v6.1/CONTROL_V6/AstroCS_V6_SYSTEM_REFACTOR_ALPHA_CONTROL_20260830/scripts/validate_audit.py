#!/usr/bin/env python3
"""Strict validator for an AstroCS V6 audit staging directory."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import pathlib
import re
import sys
from collections import Counter

from validate_task_graph import load_ledger, validate_rows

REQUIRED = {
    "00_README.md", "SUMMARY.json", "SOURCE_IDENTITY.json", "TASK_LEDGER.csv",
    "COMMITS.csv", "FINDINGS.csv", "TEST_SUMMARY.csv", "RESOURCE_SUMMARY.csv",
    "TRACEABILITY_MATRIX.csv", "LARGE_ARTIFACT_MANIFEST.csv", "PREVIEW_MANIFEST.csv",
    "MANIFEST.json", "SHA256SUMS",
}
BANNED_PARTS = {".git", "build", "testdata", "third_party", "__pycache__", ".pytest_cache", "archive", "history"}
BANNED_SUFFIXES = {".fits", ".fit", ".fts", ".obj", ".pdb", ".ilk", ".core", ".dmp", ".tar", ".gz", ".7z"}
SHA40 = re.compile(r"^[0-9a-f]{40}$")
SHA64 = re.compile(r"^[0-9a-f]{64}$")


def sha256(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def read_csv(path: pathlib.Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def validate(root: pathlib.Path) -> dict:
    root = root.resolve()
    missing = sorted(name for name in REQUIRED if not (root / name).is_file())
    if missing:
        raise ValueError(f"missing required files: {missing}")
    if not (root / "docs/review").is_dir() or not list((root / "docs/review").glob("*.md")):
        raise ValueError("docs/review is missing or empty")

    total = 0
    actual: dict[str, dict] = {}
    for path in sorted(root.rglob("*")):
        rel = path.relative_to(root)
        if path.is_symlink():
            raise ValueError(f"symlink forbidden: {rel}")
        if path.is_dir():
            continue
        if any(part in BANNED_PARTS for part in rel.parts):
            raise ValueError(f"banned path: {rel}")
        if path.suffix.lower() in BANNED_SUFFIXES:
            raise ValueError(f"banned extension: {rel}")
        size = path.stat().st_size
        if size > 5 * 1024 * 1024:
            raise ValueError(f"undeclared file >5MiB: {rel}")
        total += size
        actual[rel.as_posix()] = {"size": size, "sha256": sha256(path)}
    if total > 25 * 1024 * 1024:
        raise ValueError(f"audit staging exceeds 25MiB: {total}")

    summary = json.loads((root / "SUMMARY.json").read_text(encoding="utf-8"))
    required_summary = {"schema", "version", "verdict", "source_commit", "origin_main_commit", "control_sha256", "ledger_sha256", "task_counts", "gate_status", "science_changed", "blockers", "not_verified", "linux", "windows", "artifacts", "owner_review"}
    if set(summary) != required_summary:
        raise ValueError(f"SUMMARY fields mismatch: {sorted(set(summary)^required_summary)}")
    if summary["schema"] != "astrocs.audit-summary/v1" or summary["version"] != "0.10.0-alpha.1":
        raise ValueError("bad audit schema/version")
    if not SHA40.fullmatch(summary["source_commit"]) or summary["source_commit"] != summary["origin_main_commit"]:
        raise ValueError("source_commit must be 40hex and equal origin_main_commit")
    if not SHA64.fullmatch(summary["control_sha256"]):
        raise ValueError("bad control hash")

    ledger_path = root / "TASK_LEDGER.csv"
    ledger_rows = load_ledger(ledger_path)
    ledger_result = validate_rows(ledger_rows, require_pass_dependencies=True)
    if sha256(ledger_path) != summary["ledger_sha256"]:
        raise ValueError("ledger hash mismatch")
    counts = Counter(row["status"] for row in ledger_rows)
    if dict(sorted(counts.items())) != summary["task_counts"]:
        raise ValueError("task_counts is not derived from ledger")
    if ledger_result["gate_status"] != summary["gate_status"]:
        raise ValueError("gate_status is not derived from ledger")

    verdict = summary["verdict"]
    owner = summary["owner_review"]
    blockers = summary["blockers"]
    not_verified = summary["not_verified"]
    nonpass = {row["task_id"]: row["status"] for row in ledger_rows if row["status"] != "PASS"}
    if verdict == "READY_FOR_OWNER_REVIEW":
        if blockers or not_verified or owner != "PENDING" or nonpass not in ({"REL-004": "NOT_STARTED"}, {"REL-004": "IN_PROGRESS"}):
            raise ValueError(f"invalid READY state: nonpass={nonpass} owner={owner}")
    elif verdict == "ALPHA_RELEASE_APPROVED":
        if blockers or not_verified or owner != "APPROVED" or nonpass:
            raise ValueError("approved verdict requires all tasks PASS and owner APPROVED")
    elif verdict == "NOT_READY":
        pass
    else:
        raise ValueError(f"invalid verdict: {verdict}")

    findings = read_csv(root / "FINDINGS.csv")
    if verdict != "NOT_READY":
        open_high = [r for r in findings if r.get("severity") in {"P0", "P1"} and r.get("status") != "CLOSED"]
        if open_high:
            raise ValueError(f"open P0/P1 findings: {[r.get('finding_id') for r in open_high]}")

    heavy_pass = [r for r in ledger_rows if r["heavy_compute"] == "yes" and r["status"] == "PASS"]
    resources = read_csv(root / "RESOURCE_SUMMARY.csv")
    if heavy_pass and not resources:
        raise ValueError("heavy tasks passed but RESOURCE_SUMMARY is empty")
    if verdict != "NOT_READY" and any(r.get("gate_status") != "PASS" for r in resources):
        raise ValueError("resource summary contains non-PASS gate")

    manifest = json.loads((root / "MANIFEST.json").read_text(encoding="utf-8"))
    listed = {item["path"]: item for item in manifest.get("files", [])}
    content_paths = set(actual) - {"MANIFEST.json", "SHA256SUMS"}
    if set(listed) != content_paths:
        raise ValueError("manifest file set mismatch")
    for name in content_paths:
        if listed[name].get("sha256") != actual[name]["sha256"] or listed[name].get("size") != actual[name]["size"]:
            raise ValueError(f"manifest mismatch: {name}")

    sums = {}
    for line in (root / "SHA256SUMS").read_text(encoding="utf-8").splitlines():
        if line.strip():
            digest, name = line.split("  ", 1)
            sums[name] = digest
    if set(sums) != content_paths:
        raise ValueError("SHA256SUMS file set mismatch")
    for name in content_paths:
        if sums[name] != actual[name]["sha256"]:
            raise ValueError(f"checksum mismatch: {name}")

    return {"files": len(actual), "bytes": total, "tasks": len(ledger_rows), "verdict": verdict}


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser()
    p.add_argument("root", type=pathlib.Path)
    args = p.parse_args(argv)
    try:
        r = validate(args.root)
        print(f"AUDIT_PACKAGE_PASS files={r['files']} tasks={r['tasks']} bytes={r['bytes']} verdict={r['verdict']}")
        return 0
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"AUDIT_PACKAGE_FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
