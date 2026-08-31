#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_source_index_v61.py — R0-002 完整自有源码清单生成器。

解析根 CMakeLists.txt / tests/unit/CMakeLists.txt / cli/CMakeLists.txt / cmake/*.cmake
中全部 add_library/add_executable 显式源引用，逐项证明存在；并枚举自有源码
(cli/include/lib/cmake/schemas/tools/docs/tests/scripts)，生成：

  evidence/v6_1_rework/tasks/R0-002/SOURCE_INDEX.csv
      path,kind,owner,target,sha256,size_bytes,generated,required_for_build
  evidence/v6_1_rework/tasks/R0-002/TARGET_SOURCE_GRAPH.json
  evidence/v6_1_rework/tasks/R0-002/TARGET_SOURCE_GRAPH.dot

第三方(vendored)单列 third_party_sources.csv，不进入自有 SOURCE_INDEX。
任何生产 target 引用的自有源码缺失 → 非零退出（R0-002 验收负例）。

用法: python3 tools/quality/gen_source_index_v61.py [--out-dir DIR] [--repo ROOT]
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import sys
from pathlib import Path

SELF_OWNED_ROOTS = ("cli", "include", "lib", "cmake", "schemas", "tools",
                    "tests", "docs", "scripts", "launch")
EXCLUDE_DIRS = {".git", "build", "run", "testdata", "artifacts", "evidence",
                "reports", "third_party", "__pycache__", "工程控制",
                "AstroCS.wiki", "BASS DR3", "archive"}
VENDORED_PARTS = {"third_party"}
GENERATED_SUFFIX = {".png", ".svg", ".jpg", ".json", ".dot"}
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".inl",
                   ".in", ".cmake", ".txt", ".py", ".md", ".csv", ".yaml",
                   ".yml", ".json", ".sh", ".ps1", ".template"}


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_target_sources(cmake_text: str) -> dict[str, list[str]]:
    """Parse add_library/add_executable blocks: target -> explicit sources."""
    targets: dict[str, list[str]] = {}
    pattern = re.compile(
        r"add_(?:library|executable)\s*\(\s*([A-Za-z0-9_+.-]+)"
        r"((?:[^()]|\([^)]*\))*?)\)",
        re.S,
    )
    for match in pattern.finditer(cmake_text):
        name = match.group(1)
        body = match.group(2)
        # strip leading target options (STATIC/SHARED/INTERFACE/EXCLUDE_FROM_ALL...)
        tokens = body.split()
        sources: list[str] = []
        started = False
        for token in tokens:
            tok = token.strip()
            if not tok:
                continue
            if tok in {"STATIC", "SHARED", "INTERFACE", "OBJECT", "MODULE",
                       "EXCLUDE_FROM_ALL"}:
                continue
            if tok.startswith("${"):
                continue
            if tok.endswith(".cpp") or tok.endswith(".c") or tok.endswith(".cc") \
               or tok.endswith(".cxx") or tok.endswith(".h") or tok.endswith(".hpp"):
                sources.append(tok)
            if tok.endswith(".inl"):
                sources.append(tok)
        targets[name] = sources
    return targets


def walk_self_owned(root: Path) -> list[Path]:
    files: list[Path] = []
    for top in SELF_OWNED_ROOTS:
        base = root / top
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if not path.is_file():
                continue
            rel = path.relative_to(root)
            if any(part in EXCLUDE_DIRS for part in rel.parts):
                continue
            if any(part in VENDORED_PARTS for part in rel.parts):
                continue
            if path.suffix.lower() in SOURCE_SUFFIXES or path.name == "VERSION":
                files.append(path)
    # root-level self-owned files
    for name in ("VERSION", "AGENTS.md", "README.md", "CMakeLists.txt",
                 "CHANGELOG.md", "DEPENDENCIES.md", "HANDOVER.md",
                 "AstroCS_ENGINEERING_CONSTRAINTS.md"):
        path = root / name
        if path.is_file():
            files.append(path)
    return sorted(set(files), key=lambda p: p.relative_to(root).as_posix())


def classify_kind(rel: str) -> str:
    first = rel.split("/", 1)[0]
    if rel.startswith("cli/"):
        return "cli"
    if rel.startswith("include/"):
        return "public_header"
    if rel.startswith("lib/"):
        return "library_source"
    if rel.startswith("cmake/"):
        return "cmake"
    if rel.startswith("schemas/"):
        return "schema"
    if rel.startswith("tools/"):
        return "tool"
    if rel.startswith("tests/"):
        return "test"
    if rel.startswith("docs/"):
        return "doc"
    if rel.startswith("scripts/"):
        return "script"
    if rel.startswith("launch/"):
        return "launch"
    return "root"


def classify_owner(rel: str) -> str:
    first = rel.split("/", 1)[0]
    if first == "cli":
        return "astrocs-cli"
    if first == "include":
        return "astrocs-api"
    if first in {"cmake", "schemas", "scripts"}:
        return "astrocs-build"
    if first == "tools":
        return "astrocs-tools"
    if first == "tests":
        return "astrocs-test"
    if first == "docs":
        return "astrocs-docs"
    if first == "lib":
        parts = rel.split("/")
        if len(parts) >= 2:
            return f"astrocs-{parts[1]}"
        return "astrocs-lib"
    return "astrocs-root"


def target_of(rel: str, target_map: dict[str, list[str]]) -> str:
    """Find which target references this file, if any."""
    if rel.endswith((".cpp", ".c", ".cc", ".cxx")):
        for target, sources in target_map.items():
            if rel in sources:
                return target
    return ""


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--out-dir", type=Path, default=None)
    args = parser.parse_args(argv)

    root: Path = args.repo.resolve()
    if not (root / "CMakeLists.txt").is_file():
        print(f"SOURCE_INDEX_FAIL: {root} has no root CMakeLists.txt", file=sys.stderr)
        return 2

    out_dir = args.out_dir.resolve() if args.out_dir else root / "evidence" / "v6_1_rework" / "tasks" / "R0-002"
    out_dir.mkdir(parents=True, exist_ok=True)

    # 1. parse CMake target sources
    target_map: dict[str, list[str]] = {}
    cmake_files = [root / "CMakeLists.txt"]
    for sub in ("tests/unit/CMakeLists.txt", "cli/CMakeLists.txt"):
        p = root / sub
        if p.is_file():
            cmake_files.append(p)
    for p in sorted((root / "cmake").glob("*.cmake")):
        cmake_files.append(p)
    for cmake_path in cmake_files:
        if not cmake_path.is_file():
            continue
        parsed = parse_target_sources(cmake_path.read_text(encoding="utf-8", errors="replace"))
        for target, sources in parsed.items():
            resolved = []
            for src in sources:
                if src.startswith("${"):
                    resolved.append(src)
                    continue
                if src.startswith("/") or src.startswith(("third_party/", "lib/astro_image_io/third_party/")):
                    resolved.append(src)
                    continue
                # resolve relative to the CMake file's directory
                candidate = (cmake_path.parent / src).resolve()
                try:
                    rel = candidate.relative_to(root)
                    resolved.append(rel.as_posix())
                except ValueError:
                    resolved.append(src)
            target_map.setdefault(target, []).extend(resolved)

    # 2. verify every production target source exists
    missing: list[str] = []
    for target, sources in target_map.items():
        for src in sources:
            if src.startswith("${"):
                continue
            candidate = root / src
            if not candidate.is_file():
                missing.append(f"{target}:{src}")
    if missing:
        print("SOURCE_INDEX_FAIL missing target sources: "
              + ", ".join(sorted(set(missing))[:30]), file=sys.stderr)
        return 1

    # 3. walk self-owned files
    files = walk_self_owned(root)

    # 4. write SOURCE_INDEX.csv (validator-compatible columns)
    csv_path = out_dir / "SOURCE_INDEX.csv"
    build_required: set[str] = {"CMakeLists.txt", "VERSION", "cli/version_generated.h.in",
                                "cli/main.cpp", "cli/version_generated.h"}
    for target, sources in target_map.items():
        if target.endswith("_test"):
            continue
        for src in sources:
            if src.startswith("${"):
                continue
            build_required.add(src)
    rows = []
    for path in files:
        rel = path.relative_to(root).as_posix()
        size = path.stat().st_size
        rows.append({
            "path": rel,
            "kind": classify_kind(rel),
            "owner": classify_owner(rel),
            "target": target_of(rel, target_map),
            "sha256": sha256(path),
            "size_bytes": str(size),
            "generated": "yes" if path.suffix.lower() in GENERATED_SUFFIX else "no",
            "required_for_build": "yes" if rel in build_required else "no",
        })
    with csv_path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    # 5. TARGET_SOURCE_GRAPH.json + .dot
    graph = {
        "schema": "astrocs.target-source-graph/v1",
        "source_commit": _git_commit(root),
        "targets": {target: sorted(set(sources)) for target, sources in target_map.items()},
    }
    (out_dir / "TARGET_SOURCE_GRAPH.json").write_text(
        json.dumps(graph, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    dot_lines = ["digraph astrocs_targets {"]
    for target, sources in sorted(graph["targets"].items()):
        for src in sorted(set(sources)):
            if src.startswith("${"):
                continue
            dot_lines.append(f'  "{target}" -> "{src}";')
    dot_lines.append("}")
    (out_dir / "TARGET_SOURCE_GRAPH.dot").write_text("\n".join(dot_lines) + "\n", encoding="utf-8")

    # 6. third-party separately
    vendored: list[Path] = []
    for path in (root / "lib").rglob("*"):
        if path.is_file() and "third_party" in path.parts:
            vendored.append(path)
    with (out_dir / "third_party_sources.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["path", "size_bytes", "sha256", "license_hint"])
        for path in sorted(vendored, key=lambda p: p.relative_to(root).as_posix()):
            rel = path.relative_to(root).as_posix()
            writer.writerow([rel, path.stat().st_size, sha256(path), "cfitsio (NASA)"])

    print(f"SOURCE_INDEX_PASS targets={len(target_map)} files={len(rows)} "
          f"missing={len(missing)} vendored={len(vendored)}")
    print(f"  SOURCE_INDEX.csv    -> {csv_path}")
    print(f"  TARGET_SOURCE_GRAPH -> {out_dir / 'TARGET_SOURCE_GRAPH.json'}")
    return 0


def _git_commit(root: Path) -> str:
    import subprocess
    try:
        out = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"],
                             capture_output=True, text=True, timeout=30)
        return out.stdout.strip()
    except Exception:
        return "0000000000000000000000000000000000000000"


if __name__ == "__main__":
    raise SystemExit(main())
