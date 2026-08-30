#!/usr/bin/env python3
"""DATA-001 DataArtifact 注册表校验器。
检查 docs/contracts/DATA_ARTIFACTS.md:
  - schema_id 唯一且格式合法;
  - DATA_SEMANTICS.md 声明的 DATA-* ID 全部登记;
  - 每个 schema 含 scalar/shape/unit/coordinate/invalid/ownership/serialization 字段。
exit 0 = PASS; 违例 => 非 0。
"""
from __future__ import annotations

import argparse
import pathlib
import re
import sys

DATA_RE = re.compile(r"^DATA-[A-Z0-9-]+-\d{3}$")
DATA_FIND_RE = re.compile(r"\bDATA-[A-Z0-9-]+-\d{3}\b")
REQUIRED_COLS = {"schema_id", "内容", "scalar", "shape/axis", "unit", "coordinate", "invalid", "ownership", "serialization"}


def parse_table(path: pathlib.Path) -> list[dict[str, str]]:
    """只解析第一个以 schema_id 开头且含 '内容' 列的表格(schema 清单)。"""
    rows: list[dict[str, str]] = []
    with path.open(encoding="utf-8") as f:
        lines = f.readlines()
    in_table = False
    header: list[str] = []
    for line in lines:
        stripped = line.lstrip()
        if stripped.startswith("| "):
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if not in_table:
                if "schema_id" not in cells:
                    continue
                header = cells
                in_table = True
                continue
            if len(cells) == len(header):
                if all(set(c) <= {"-"} for c in cells):
                    continue
                rows.append(dict(zip(header, cells)))
        elif stripped.startswith("|---") or stripped == "|---|" or (in_table and stripped.startswith("|")):
            continue
        else:
            in_table = False
    return rows


def extract_data_ids(text: str) -> set[str]:
    return set(DATA_FIND_RE.findall(text))


def validate(root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    artifacts = root / "docs/contracts/DATA_ARTIFACTS.md"
    semantics = root / "docs/contracts/DATA_SEMANTICS.md"
    if not artifacts.is_file():
        return ["docs/contracts/DATA_ARTIFACTS.md 不存在"]
    rows = parse_table(artifacts)
    if not rows:
        return ["DATA_ARTIFACTS.md 无表格行"]
    seen: set[str] = set()
    for r in rows:
        sid = r.get("schema_id", "")
        if not DATA_RE.fullmatch(sid):
            errors.append(f"非法 schema_id: {sid!r}")
        if sid in seen:
            errors.append(f"重复 schema_id: {sid}")
        seen.add(sid)
        for col in REQUIRED_COLS:
            if col not in r or not r[col].strip():
                errors.append(f"{sid}: 缺字段 {col}")
    if semantics.is_file():
        declared = extract_data_ids(semantics.read_text(encoding="utf-8"))
        missing = declared - seen
        if missing:
            errors.append(f"DATA_SEMANTICS 声明但未登记: {sorted(missing)}")
    return errors


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser()
    p.add_argument("root", type=pathlib.Path, nargs="?", default=pathlib.Path("."))
    args = p.parse_args(argv)
    try:
        errors = validate(args.root.resolve())
    except OSError as exc:
        print(f"DATA_ARTIFACTS_FAIL: {exc}", file=sys.stderr)
        return 1
    if errors:
        print(f"DATA_ARTIFACTS_FAIL ({len(errors)}):")
        for e in errors[:30]:
            print(" ", e)
        return 1
    print(f"DATA_ARTIFACTS_PASS schemas={len(parse_table(args.root.resolve() / 'docs/contracts/DATA_ARTIFACTS.md'))}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
