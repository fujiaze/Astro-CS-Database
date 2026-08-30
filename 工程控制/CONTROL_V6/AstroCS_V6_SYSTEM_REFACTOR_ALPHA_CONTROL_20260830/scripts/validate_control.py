#!/usr/bin/env python3
"""Validate the immutable AstroCS V6 control package."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import sys

from validate_task_graph import LedgerError, load_ledger, validate_rows

EXPECTED_CONSTRAINT_SHA = "dc47fbbbeccb239fc1834a985b357d6ded50080977708b6ea5c9811bf4de4b92"
REQUIRED = [
    "00_READ_FIRST.md", "01_ASTROCS_ENGINEERING_CONSTRAINTS.md", "02_BASELINE_AUDIT.md",
    "03_TARGET_ARCHITECTURE.md", "04_MIGRATION_AND_GATES.md", "05_TASK_LEDGER.csv",
    "06_TASK_SPECIFICATIONS.md", "07_SCIENCE_AND_TEST_MATRIX.md", "08_CPU_PARALLEL_BACKEND.md",
    "09_DOCUMENTATION_AND_MACHINE_CHECKS.md", "10_LINUX_WINDOWS_EXECUTION.md", "11_GIT_MAIN_ONLY.md",
    "12_AUDIT_PACKAGE_SPEC.md", "13_RELEASE_ACCEPTANCE.md", "14_PRIMARY_REFERENCES.md",
    "AGENTS_REPLACEMENT.md", "START_PROMPT.txt", "MANIFEST.json", "SHA256SUMS",
    "schemas/module_descriptor.schema.json", "schemas/pipeline_ir.schema.json",
    "schemas/data_artifact.schema.json", "schemas/task_result.schema.json",
    "schemas/audit_summary.schema.json", "schemas/cpu_profile.schema.json",
    "scripts/validate_task_graph.py", "scripts/validate_control.py", "scripts/validate_audit.py",
    "scripts/package_audit.py", "scripts/selftest.py",
]
BANNED_PARTS = {".git", "build", "testdata", "__pycache__", ".pytest_cache"}
BANNED_SUFFIXES = {".fits", ".fit", ".fts", ".obj", ".pdb", ".ilk", ".dll", ".so", ".exe", ".core", ".dmp"}


def digest(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def fail(msg: str) -> None:
    raise ValueError(msg)


def validate(root: pathlib.Path) -> dict:
    root = root.resolve()
    for rel in REQUIRED:
        if not (root / rel).is_file():
            fail(f"missing required file: {rel}")

    if digest(root / "01_ASTROCS_ENGINEERING_CONSTRAINTS.md") != EXPECTED_CONSTRAINT_SHA:
        fail("engineering constraints changed")

    prompt = (root / "START_PROMPT.txt").read_text(encoding="utf-8").strip()
    if not prompt or len(prompt) > 100 or "\n" in prompt:
        fail(f"startup prompt must be one line and <=100 Unicode chars; got {len(prompt)}")

    for schema in sorted((root / "schemas").glob("*.json")):
        data = json.loads(schema.read_text(encoding="utf-8"))
        if data.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
            fail(f"bad schema declaration: {schema.name}")

    ledger_result = validate_rows(load_ledger(root / "05_TASK_LEDGER.csv"), require_pass_dependencies=True)
    if ledger_result["task_count"] < 80:
        fail(f"task ledger unexpectedly short: {ledger_result['task_count']}")
    if ledger_result["topological_order"][-1] != "REL-004":
        fail("REL-004 must be the final task")

    files = []
    total = 0
    for path in sorted(root.rglob("*")):
        rel = path.relative_to(root)
        if path.is_symlink():
            fail(f"symlink forbidden: {rel}")
        if path.is_dir():
            continue
        if any(part in BANNED_PARTS for part in rel.parts):
            fail(f"banned path: {rel}")
        if path.suffix.lower() in BANNED_SUFFIXES:
            fail(f"banned binary/data suffix: {rel}")
        size = path.stat().st_size
        if size > 5 * 1024 * 1024:
            fail(f"file too large: {rel}")
        total += size
        files.append({"path": rel.as_posix(), "size": size, "sha256": digest(path)})
    if total > 25 * 1024 * 1024:
        fail(f"control package too large: {total}")

    manifest = json.loads((root / "MANIFEST.json").read_text(encoding="utf-8"))
    if manifest.get("schema") != "astrocs.control-manifest/v1":
        fail("bad manifest schema")
    if manifest.get("target_version") != "0.10.0-alpha.1":
        fail("bad target version")
    if manifest.get("baseline_commit") != "587fe0e341a780da726917f40ed77f610de0c73f":
        fail("bad baseline commit")

    checksum_lines = (root / "SHA256SUMS").read_text(encoding="utf-8").splitlines()
    expected = {}
    for line in checksum_lines:
        if not line.strip():
            continue
        try:
            sha, name = line.split("  ", 1)
        except ValueError as exc:
            raise ValueError(f"bad SHA256SUMS line: {line!r}") from exc
        if name in {"SHA256SUMS", "MANIFEST.json"}:
            fail(f"self-referential checksum entry forbidden: {name}")
        if name in expected:
            fail(f"duplicate checksum entry: {name}")
        expected[name] = sha
    actual_names = {item["path"] for item in files} - {"SHA256SUMS", "MANIFEST.json"}
    if set(expected) != actual_names:
        fail(f"checksum file set mismatch missing={sorted(actual_names-set(expected))} extra={sorted(set(expected)-actual_names)}")
    for name, sha in expected.items():
        if digest(root / name) != sha:
            fail(f"checksum mismatch: {name}")

    listed = {item["path"]: item for item in manifest.get("files", [])}
    if set(listed) != actual_names:
        fail("manifest file set mismatch")
    for name in actual_names:
        if listed[name].get("sha256") != digest(root / name) or listed[name].get("size") != (root / name).stat().st_size:
            fail(f"manifest mismatch: {name}")

    return {"files": len(files), "bytes": total, "tasks": ledger_result["task_count"], "prompt_chars": len(prompt)}


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser()
    p.add_argument("root", nargs="?", default=".", type=pathlib.Path)
    args = p.parse_args(argv)
    try:
        result = validate(args.root)
        print(f"CONTROL_PASS files={result['files']} tasks={result['tasks']} bytes={result['bytes']} prompt_chars={result['prompt_chars']}")
        return 0
    except (OSError, ValueError, LedgerError, json.JSONDecodeError) as exc:
        print(f"CONTROL_FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
