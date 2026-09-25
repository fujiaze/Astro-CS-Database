#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/tools/file_audit.py —— 文件审计（分子与分母由**同一条命令**产出）。

存在理由（一页纸 S2-B）
  审查节点原话：`tool_missing=true` 与 `coverage_ok=true` 并存、**分母 713 全仓无定义**、
  `eng/tools/file_audit` 缺失。三者同一根因：**覆盖度结论的分子与分母来自不同来源**
  —— 分母是一次口头目标（"700+"／"713"），分子是另一支 `find+wc` 替代统计，工具本身
  又不存在，于是结论无法被任何一条命令复算。

判据（S2-B「判完成」第 2 条）
  **每个分母与分子由同一命令产出**。本工具因此：
    1. 用**一条** `git ls-files -z` 同时取得分子与分母（同一命令、同一快照），
       并把该命令原文写进产物 `source_command`，供任何第三方逐字复跑；
    2. 分母**有定义**且定义随产物落盘（`denominator_definition`），不使用口头阈值；
    3. 无法执行该命令（不在 git 工作树 / git 不可用）⇒ **exit 2 且不写产物**，
       fail-closed；绝不退回 `find+wc` 之类的第二来源（那正是本工具要消灭的形态）。

分母口径（本文件即定义正本）
  分母 = `shipping_files`：`git ls-files` 中属于**出货语料**的已跟踪文件
         （前缀 lib/ eng/ docs/ 实验/，或根级清单 SHIPPING_EXACT 中的条目）。
         第三方、testdata、gaia、artifacts、run、工程控制 等**不属于**出货语料。
  分子 = `standard_scanned`：上述文件中扩展名落在 SCANNED_EXT 内的那些
         （即"标准扫描器真的会读"的文件）。`unscanned` 列出未覆盖项，使分子可核对。

用法
  python3 eng/tools/file_audit.py [--root DIR] [--json-out PATH] [--quiet]
退出码 0 = 产出审计；2 = 前置命令不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from collections import Counter

SOURCE_COMMAND = ["git", "ls-files", "-z"]

SHIPPING_PREFIXES = ("lib/", "eng/", "docs/", "实验/")
SHIPPING_EXACT = (
    "CMakeLists.txt", "CMakePresets.json", "VERSION", "README.md", "AGENTS.md",
    "ASTROCS_DESIGN.md", "ENGINEERING_SPEC.md", "ACCEPTANCE_SPEC.md",
    "CONTROL_PACK_SPEC.md", "DEPENDENCIES.md", "memory.md",
)
SCANNED_EXT = (
    ".py", ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inc",
    ".md", ".rst", ".txt", ".json", ".jsonl", ".yaml", ".yml", ".toml", ".ini",
    ".cmake", ".sh", ".ps1", ".bat", ".csv", ".fits", ".xisf",
)

DENOMINATOR_DEFINITION = (
    "分母 = source_command 的 stdout（NUL 分隔）中，路径满足："
    "(a) 以 lib/ eng/ docs/ 实验/ 之一开头，或 (b) 恰为根级出货清单条目。"
    "分母 = 满足条件的已跟踪文件数；分子 = 其中扩展名 ∈ SCANNED_EXT 者。"
    "分子与分母同一次 git ls-files 调用、同一快照，故比值可由该命令复算。"
)


def tracked_files(root):
    """一条命令取全量已跟踪文件。失败即抛（调用方转 exit 2）。"""
    r = subprocess.run(SOURCE_COMMAND, cwd=root, capture_output=True)
    if r.returncode != 0:
        raise RuntimeError("git ls-files 失败 rc=%d: %s"
                           % (r.returncode, r.stderr.decode("utf-8", "replace")[:300]))
    out = r.stdout.decode("utf-8", "surrogateescape")
    return [p for p in out.split("\0") if p]


def is_shipping(path):
    return path.startswith(SHIPPING_PREFIXES) or path in SHIPPING_EXACT


def audit(root):
    files = tracked_files(root)
    shipping = sorted(p for p in files if is_shipping(p))
    scanned = [p for p in shipping if os.path.splitext(p)[1].lower() in SCANNED_EXT]
    unscanned = sorted(p for p in shipping if p not in set(scanned))
    ext = Counter(os.path.splitext(p)[1].lower() or "<none>" for p in shipping)
    return {
        "tool": "file_audit",
        "schema_version": 1,
        "root": os.path.abspath(root),
        "source_command": SOURCE_COMMAND,
        "source_command_text": " ".join(SOURCE_COMMAND),
        "denominator_definition": DENOMINATOR_DEFINITION,
        "counts": {
            "tracked_total": len(files),
            "shipping_total": len(shipping),
            "standard_scanned": len(scanned),
            "coverage": (len(scanned) / len(shipping)) if shipping else 0.0,
        },
        "by_extension": dict(sorted(ext.items())),
        "unscanned": unscanned,
        "produced_by": "eng/tools/file_audit.py",
    }


def main(argv=None):
    ap = argparse.ArgumentParser(description="文件审计（分子/分母同源）")
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))))
    ap.add_argument("--json-out", default="")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args(argv)
    try:
        out = audit(a.root)
    except (OSError, RuntimeError) as exc:
        print("FILE_AUDIT_ERROR: %s（fail-closed，不退回第二来源）" % exc, file=sys.stderr)
        return 2
    if a.json_out:
        d = os.path.dirname(os.path.abspath(a.json_out))
        if d:
            os.makedirs(d, exist_ok=True)
        with open(a.json_out, "w", encoding="utf-8") as f:
            json.dump(out, f, ensure_ascii=False, indent=1, sort_keys=True)
        print("REPORT_WRITTEN %s" % a.json_out)
    if not a.quiet:
        print(json.dumps(out, ensure_ascii=False, indent=1, sort_keys=True))
    else:
        c = out["counts"]
        print("FILE_AUDIT ok shipping=%d scanned=%d coverage=%.4f cmd=%s"
              % (c["shipping_total"], c["standard_scanned"], c["coverage"],
                 out["source_command_text"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
