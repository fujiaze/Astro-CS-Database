#!/usr/bin/env python3
"""DOC-001 合同 ID 注册表校验器。
检查 docs/contracts/INDEX.yaml:
  - ID 唯一、格式合法;
  - 每个 path 存在;
  - owner 非空;
  - upstream/downstream 引用存在且双向一致;
  - ACTIVE 不得依赖 OBSOLETE/CONFLICT。
exit 0 = PASS; 任何违例 => 非 0 (negative fixtures 必须失败)。
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

try:
    import yaml
except ImportError:
    print("CONTRACT_GRAPH_FAIL: 需要 PyYAML (pip install pyyaml)", file=sys.stderr)
    sys.exit(2)

ID_RE = re.compile(r"^(SCI|ALG|DATA|ARCH|API|MOD|TEST|TST)-[A-Z0-9-]+$")
STATUSES = {"ACTIVE", "DRAFT", "OBSOLETE", "CONFLICT", "DORMANT"}


def load_index(path: pathlib.Path) -> dict:
    with path.open(encoding="utf-8") as f:
        doc = yaml.safe_load(f)
    if not isinstance(doc, dict) or doc.get("schema") != "astrocs.contract-index/v1":
        raise ValueError("INDEX.yaml schema 声明缺失或错误")
    return doc


def validate(root: pathlib.Path, index_path: pathlib.Path) -> list[str]:
    errors: list[str] = []
    doc = load_index(index_path)
    contracts = doc.get("contracts", [])
    by_id: dict[str, dict] = {}
    for i, c in enumerate(contracts):
        cid = c.get("id", "")
        if not ID_RE.fullmatch(cid):
            errors.append(f"[{i}] 非法 ID: {cid!r}")
        if cid in by_id:
            errors.append(f"[{i}] 重复 ID: {cid}")
        st = c.get("status", "")
        if st not in STATUSES:
            errors.append(f"[{i}] {cid}: 非法状态 {st!r}")
        if not c.get("owner"):
            errors.append(f"[{i}] {cid}: owner 为空")
        p = c.get("path", "")
        if p and not (root / p).is_file():
            errors.append(f"[{i}] {cid}: 路径不存在 {p}")
        by_id[cid] = c
    for c in contracts:
        cid = c["id"]
        for dep in c.get("upstream", []) + c.get("downstream", []):
            if dep not in by_id:
                errors.append(f"{cid}: 悬空引用 {dep}")
    # 双向一致性: downstream 反向必须出现在 upstream
    for c in contracts:
        cid = c["id"]
        for d in c.get("downstream", []):
            if cid not in by_id.get(d, {}).get("upstream", []):
                errors.append(f"{cid}: downstream {d} 未在 {d}.upstream 反向声明")
    # ACTIVE 不得依赖 OBSOLETE/CONFLICT
    for c in contracts:
        if c.get("status") == "ACTIVE":
            for dep in c.get("upstream", []):
                ds = by_id.get(dep, {}).get("status")
                if ds in ("OBSOLETE", "CONFLICT"):
                    errors.append(f"{cid}: ACTIVE 依赖 {ds} 的 {dep}")
    return errors


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser()
    p.add_argument("root", type=pathlib.Path, nargs="?", default=pathlib.Path("."))
    p.add_argument("--index", type=pathlib.Path, default=pathlib.Path("docs/contracts/INDEX.yaml"))
    args = p.parse_args(argv)
    root = args.root.resolve()
    try:
        errors = validate(root, (root / args.index) if not args.index.is_absolute() else args.index)
    except (OSError, ValueError) as exc:
        print(f"CONTRACT_GRAPH_FAIL: {exc}", file=sys.stderr)
        return 1
    if errors:
        print(f"CONTRACT_GRAPH_FAIL ({len(errors)}):")
        for e in errors[:30]:
            print(" ", e)
        return 1
    n = len(load_index((root / args.index) if not args.index.is_absolute() else args.index).get("contracts", []))
    print(f"CONTRACT_GRAPH_PASS contracts={n}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
