#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/check_psfsw_retired.py — psfsw_robust_weight 退役面静态门（PSFSW-RETIRED-STATIC）。

原实现是注册表内联的 bash 一行（bash -lc "! grep -rn TOKEN <root> | grep -v 退役标记"），
违反注册表校验器 R4（command[0] 必须 ∈ {python3, python}），使
eng/ci/validate_registry.py --strict 长期红。本脚本是**语义等价**的 Python 落地，
并补齐 fail-closed 与可执行正负例（ENGINEERING_SPEC §10）：

判据：eng/contracts/schemas/unified/** 下任何一行出现 psfsw_robust_weight
      且该行不含 已退役 / retired 标记 ⇒ 判红。
fail-closed：扫描根缺失（锚失效）⇒ rc=2；不可读文件 ⇒ rc=2（绝不当作无违规）。

用法:
  python3 eng/ci/check_psfsw_retired.py [--root eng/contracts/schemas/unified]
  python3 eng/ci/check_psfsw_retired.py --self-test
exit 0 = 无未标注残留；1 = 存在未标注残留；2 = 输入不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import pathlib
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_ROOT = "eng/contracts/schemas/unified"
TOKEN = "psfsw_robust_weight"
RETIRED_MARKERS = ("已退役", "retired")


def scan(root: pathlib.Path) -> tuple[int, list[str], str | None]:
    """返回 (rc, 命中行, 错误)。rc 语义同 main。"""
    if not root.is_dir():
        return 2, [], f"扫描根缺失（锚失效, fail-closed）：{root}"
    hits: list[str] = []
    try:
        files = sorted(p for p in root.rglob("*") if p.is_file())
    except OSError as exc:
        return 2, [], f"扫描根不可遍历：{root}（{exc}）"
    for path in files:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            return 2, [], f"文件不可读（fail-closed）：{path}（{exc}）"
        for lineno, line in enumerate(text.splitlines(), 1):
            if TOKEN not in line:
                continue
            # 与原 bash 口径一致：过滤作用于 grep 的整行输出（含 文件路径:行号: 前缀），
            # 故路径里带 retired/已退役 的负例文件（n5_retired_*.schema-violation.json）
            # 与行内标注同样放行。
            grep_line = f"{path}:{lineno}:{line}"
            if any(m in grep_line for m in RETIRED_MARKERS):
                continue
            hits.append(f"{path}:{lineno}: {line.strip()[:160]}")
    return (1 if hits else 0), hits, None


def self_test() -> int:
    cases: list[tuple[str, str, bool]] = []
    with tempfile.TemporaryDirectory() as tmp:
        base = pathlib.Path(tmp)
        clean = base / "clean"
        (clean / "sub").mkdir(parents=True)
        (clean / "a.json").write_text('{"note": "no token here"}\n', encoding="utf-8")
        (clean / "sub" / "b.json").write_text(
            '{"x": "psfsw_robust_weight 已退役"}\n{"y": "psfsw_robust_weight retired"}\n',
            encoding="utf-8")
        rc, hits, err = scan(clean)
        cases.append(("G1", "干净树（含已退役标注行）-> rc=0", rc == 0 and not err))
        dirty = base / "dirty"
        dirty.mkdir()
        (dirty / "c.json").write_text('{"x": "psfsw_robust_weight"}\n', encoding="utf-8")
        rc2, hits2, err2 = scan(dirty)
        cases.append(("N1", "未标注残留 -> rc=1 且点名文件:行",
                      rc2 == 1 and bool(hits2) and "c.json" in hits2[0]))
        rc3, _, err3 = scan(base / "absent")
        cases.append(("N2", "扫描根缺失 -> rc=2（fail-closed，绝不判绿）",
                      rc3 == 2 and bool(err3)))
    passed = sum(1 for _, _, ok in cases if ok)
    for tag, name, ok in cases:
        print(f"  [{tag}] {name}: {'OK' if ok else 'MISMATCH'}")
    print(f"PSFSW_RETIRED_SELFTEST: cases={len(cases)} passed={passed} "
          f"failed={len(cases) - passed}")
    if passed != len(cases):
        print("PSFSW_RETIRED_SELFTEST_FAIL", file=sys.stderr)
        return 1
    print("PSFSW_RETIRED_SELFTEST_PASS: 正例绿 / 残留红 / 锚缺失 fail-closed")
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="psfsw_robust_weight 退役面静态门")
    ap.add_argument("--root", default=DEFAULT_ROOT, help=f"扫描根（默认 {DEFAULT_ROOT}）")
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    root = pathlib.Path(args.root)
    if not root.is_absolute():
        root = REPO / root
    rc, hits, err = scan(root)
    if err:
        print(f"PSFSW_RETIRED_FAIL: {err}", file=sys.stderr)
        return rc
    if hits:
        print("PSFSW_RETIRED_FAIL: 未标注退役标记的 psfsw_robust_weight 残留：")
        for h in hits:
            print("  " + h)
        return rc
    print(f"PSFSW_RETIRED_PASS: {root.relative_to(REPO) if root.is_relative_to(REPO) else root}"
          f" 无未标注 psfsw_robust_weight 残留")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
