#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import shutil
import sys

ROOT_FILES = {
    "00_READ_FIRST.md", "FINAL_REPORT.md", "SUMMARY.json", "TASK_LEDGER.csv",
    "COMMITS.csv", "FINDINGS.csv", "BUILD_RESULTS.csv", "TEST_RESULTS.csv",
    "CPU_PROFILE_RESULTS.csv", "RESOURCE_RESULTS.csv", "LARGE_ARTIFACTS.csv",
    "TRACEABILITY.csv", "REVIEW_CAPSULE_INDEX.csv", "SCIENCE_CLAIMS.csv",
    "RELEASE_ARTIFACTS.csv", "CHECKPOINTS.csv", "MANIFEST.json",
}
ALLOWED_DIRS = {"control", "docs", "source_review", "reports", "metrics", "screenshots", "capsule_index"}
ALLOWED_SUFFIXES = {".md", ".txt", ".json", ".jsonl", ".csv", ".yaml", ".yml", ".h", ".hpp", ".c", ".cc", ".cpp", ".py", ".sh", ".png", ".svg"}
FORBIDDEN_PARTS = {".git", "build", "builds", "cache", "tmp", "temp", "testdata", "hips"}
MAX_FILE = 5 * 1024 * 1024


def allowed(relative: pathlib.Path) -> bool:
    if relative.as_posix() in ROOT_FILES:
        return True
    return bool(relative.parts and relative.parts[0] in ALLOWED_DIRS and relative.suffix.lower() in ALLOWED_SUFFIXES)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: package_final.py SOURCE_DIR OUTPUT_DIR", file=sys.stderr)
        return 2
    source = pathlib.Path(sys.argv[1]).resolve()
    output = pathlib.Path(sys.argv[2]).resolve()
    if not source.is_dir() or output == source or source in output.parents:
        print("invalid source/output", file=sys.stderr)
        return 2
    if output.exists():
        print("output already exists; choose a new empty path", file=sys.stderr)
        return 2
    selected: list[tuple[pathlib.Path, pathlib.Path]] = []
    errors: list[str] = []
    for path in sorted(item for item in source.rglob("*") if item.is_file()):
        relative = path.relative_to(source)
        if any(part.lower() in FORBIDDEN_PARTS for part in relative.parts):
            errors.append(f"forbidden path: {relative}")
            continue
        if not allowed(relative):
            continue
        if path.stat().st_size == 0:
            errors.append(f"empty file: {relative}")
        elif path.stat().st_size > MAX_FILE:
            errors.append(f"file too large: {relative}")
        else:
            selected.append((path, relative))
    missing = sorted(ROOT_FILES - {relative.as_posix() for _, relative in selected})
    errors.extend(f"missing root file: {name}" for name in missing)
    if errors:
        print("PACKAGE_SELECTION_FAIL")
        for error in errors:
            print(f"- {error}")
        return 1
    output.mkdir(parents=True)
    for path, relative in selected:
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    print(f"PACKAGE_SELECTION_PASS files={len(selected)} output={output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
