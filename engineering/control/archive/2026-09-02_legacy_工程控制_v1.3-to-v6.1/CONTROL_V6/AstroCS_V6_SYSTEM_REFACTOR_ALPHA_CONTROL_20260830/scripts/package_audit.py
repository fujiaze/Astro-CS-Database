#!/usr/bin/env python3
"""Package a pre-staged, whitelist-only AstroCS audit directory."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import shutil
import tempfile
import zipfile

ALLOWED_ROOT_FILES = {
    "00_README.md", "SUMMARY.json", "SOURCE_IDENTITY.json", "TASK_LEDGER.csv",
    "COMMITS.csv", "FINDINGS.csv", "TEST_SUMMARY.csv", "RESOURCE_SUMMARY.csv",
    "TRACEABILITY_MATRIX.csv", "LARGE_ARTIFACT_MANIFEST.csv", "PREVIEW_MANIFEST.csv",
}
ALLOWED_PREFIXES = ("docs/review/", "docs/contracts/changed/", "source/changed_and_required/", "evidence/gates/", "evidence/failures/minimal/", "previews/")
BANNED_PARTS = {".git", "build", "testdata", "third_party", "__pycache__", ".pytest_cache", "archive", "history"}
BANNED_SUFFIXES = {".fits", ".fit", ".fts", ".obj", ".pdb", ".ilk", ".core", ".dmp", ".tar", ".gz", ".7z", ".zip"}
MAX_FILE = 5 * 1024 * 1024
MAX_TOTAL = 25 * 1024 * 1024


def sha256(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def allowed(rel: pathlib.PurePosixPath) -> bool:
    text = rel.as_posix()
    return text in ALLOWED_ROOT_FILES or any(text.startswith(prefix) for prefix in ALLOWED_PREFIXES)


def collect(source: pathlib.Path) -> list[tuple[pathlib.Path, pathlib.PurePosixPath]]:
    out = []
    total = 0
    for path in sorted(source.rglob("*")):
        rel = pathlib.PurePosixPath(path.relative_to(source).as_posix())
        if path.is_symlink():
            raise ValueError(f"symlink forbidden: {rel}")
        if path.is_dir():
            continue
        if not allowed(rel):
            raise ValueError(f"not whitelisted: {rel}")
        if any(part in BANNED_PARTS for part in rel.parts) or path.suffix.lower() in BANNED_SUFFIXES:
            raise ValueError(f"banned audit content: {rel}")
        size = path.stat().st_size
        if size > MAX_FILE:
            raise ValueError(f"file exceeds 5MiB: {rel}")
        total += size
        if total > MAX_TOTAL:
            raise ValueError("audit staging exceeds 25MiB")
        out.append((path, rel))
    missing = sorted(name for name in ALLOWED_ROOT_FILES if not (source / name).is_file())
    if missing:
        raise ValueError(f"missing root files: {missing}")
    if not any(rel.as_posix().startswith("docs/review/") for _, rel in out):
        raise ValueError("docs/review is empty")
    return out


def package(source: pathlib.Path, output: pathlib.Path) -> dict:
    source = source.resolve()
    output = output.resolve()
    if output.exists():
        raise ValueError(f"output already exists: {output}")
    files = collect(source)
    with tempfile.TemporaryDirectory(prefix="astrocs-audit-") as td:
        stage = pathlib.Path(td) / "audit"
        stage.mkdir()
        entries = []
        for src, rel in files:
            dst = stage / pathlib.Path(rel.as_posix())
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, dst)
            entries.append({"path": rel.as_posix(), "size": dst.stat().st_size, "sha256": sha256(dst)})
        entries.sort(key=lambda x: x["path"])
        (stage / "MANIFEST.json").write_text(json.dumps({"schema": "astrocs.audit-manifest/v1", "files": entries}, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        (stage / "SHA256SUMS").write_text("".join(f"{e['sha256']}  {e['path']}\n" for e in entries), encoding="utf-8")
        output.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(output, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
            for path in sorted(stage.rglob("*")):
                if path.is_file():
                    zf.write(path, path.relative_to(stage).as_posix())
    return {"path": str(output), "size": output.stat().st_size, "sha256": sha256(output), "files": len(files) + 2}


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("staging", type=pathlib.Path)
    p.add_argument("output", type=pathlib.Path)
    args = p.parse_args()
    try:
        result = package(args.staging, args.output)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except (OSError, ValueError) as exc:
        print(f"PACKAGE_AUDIT_FAIL: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
