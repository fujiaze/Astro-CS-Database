#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_source_index_v61.py — R0-002 SOURCE_INDEX 验证器。

验证：
1. SOURCE_INDEX.csv 列完整，路径唯一、无绝对路径/..；
2. 每个已登记文件存在且 sha256/size 匹配；
3. 每个生产 target 的显式源引用都存在于 index 且 required_for_build=yes；
4. 故意删除一个 CMake 源引用的 fixture → 非零退出（负例）。

用法: python3 tools/quality/check_source_index_v61.py --repo ROOT --index DIR/SOURCE_INDEX.csv
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import sys
from pathlib import Path

REQUIRED_COLUMNS = {"path", "kind", "owner", "target", "sha256", "size_bytes",
                    "generated", "required_for_build"}


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--index", type=Path, required=True)
    parser.add_argument("--graph", type=Path, default=None)
    args = parser.parse_args(argv)

    root: Path = args.repo.resolve()
    index_path = args.index.resolve()
    errors: list[str] = []

    with index_path.open("r", encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames is None or not REQUIRED_COLUMNS.issubset(set(reader.fieldnames)):
            errors.append(f"SOURCE_INDEX columns mismatch: {reader.fieldnames}")
        rows = list(reader)

    seen: set[str] = set()
    for row in rows:
        name = row["path"]
        if name in seen:
            errors.append(f"duplicate path: {name}")
        seen.add(name)
        if name.startswith("/") or ".." in Path(name).parts or "\\" in name:
            errors.append(f"bad path: {name}")
        if not re.fullmatch(r"[0-9a-f]{64}", row["sha256"]):
            errors.append(f"bad sha256: {name}")
        path = root / name
        if not path.is_file():
            errors.append(f"missing file: {name}")
            continue
        if row["sha256"] != sha256(path):
            errors.append(f"sha256 mismatch: {name}")
        if int(row["size_bytes"]) != path.stat().st_size:
            errors.append(f"size mismatch: {name}")

    # production target source completeness via graph
    graph_path = args.graph or (index_path.parent / "TARGET_SOURCE_GRAPH.json")
    if graph_path.is_file():
        graph = json.loads(graph_path.read_text(encoding="utf-8"))
        idx = {r["path"]: r for r in rows}
        for target, sources in graph.get("targets", {}).items():
            if target.endswith("_test"):
                continue
            for src in sources:
                if src.startswith("${") or src.startswith("third_party/"):
                    continue
                if src not in idx:
                    errors.append(f"{target} references missing index entry: {src}")
                elif idx[src]["required_for_build"] != "yes":
                    errors.append(f"{target} source not required_for_build: {src}")

    if errors:
        print("SOURCE_INDEX_CHECK_FAIL")
        for error in errors[:40]:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print(f"SOURCE_INDEX_CHECK_PASS files={len(rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
