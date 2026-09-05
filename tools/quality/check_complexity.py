#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_complexity.py — V8-CI-005 linux-deep 复杂度基线检查（owner=SA-CI-32）。

状态：占位实现（placeholder）。合同（07_CI_MACHINE_CONTRACT.md「Coverage 与
复杂度」）：第一次 deep CI 只测量并记录基线，不虚构阈值；第二个原子任务再
冻结每模块复杂度上限。本脚本即第一次基线测量器：

  * 目标（--paths，逗号分隔仓库目录）下的 C/C++ 源与头文件逐个统计：
    文件数、总行数、函数定义近似计数（正则近似，非正式解析器）；
  * 输出 JSON 基线报告到 --output（登记于 ci/checks.json outputs）；
  * 阈值固定为 null（未冻结），报告带 "placeholder": true 与所有权标注；
  * 永远 exit 0（只记录，不做门禁判定）；正式 C++ 解析器由后续质量任务
    （owner=SA-CI-32）替换，届时本文件升级为真实检查器。

仅 stdlib；无外部命令；只读扫描 + 写 --output 一个文件（run/ 临时目录）。

V8-CI-005（SA-CI-32）增强（保持占位语义：只记录、exit 0、不判阈值）：
  * 增加分支 token 计数（if/for/while/case/catch/&&/||/?:）→
    per-file cyclomatic = 1 + branch_tokens（文件级粒度；不做函数体切分，
    namespace/class 花括号会把多函数吞并成假"巨型函数"）；
  * 排除 ACR dormant 树（约束 §C：生产构建/度量不含 ACR，lib/acr/** 与
    legacy/** 不进复杂度基线，逐路径在 report.excluded 登记）。
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

# C/C++ 函数定义近似：行首到 "{") 简化正则（近似计数，仅基线用）
FUNC_RE = re.compile(
    r"^[A-Za-z_][A-Za-z0-9_:<>,&*\s]*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?\{",
)
# 分支 token（文件级圈复杂度近似；与函数计数同受字符串/注释噪声影响，仅基线用）
BRANCH_RE = re.compile(r"\b(?:if|for|while|case|catch)\b|&&|\|\||\?")
EXCLUDE_PARTS = ("lib/acr", "legacy", "third_party", "thirdparty")
# ACR dormant（约束 §C）+ 遗留隔离树 + vendored 第三方（QA-002 -w 隔离，不度量）
EXTS = (".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx")


def scan_path(base: Path, rel: str) -> dict:
    """统计一个仓库相对目录下的 C/C++ 文件基线（ACR/legacy 排除）。"""
    root = base / rel
    out = {"path": rel, "exists": root.is_dir(), "files": 0, "lines": 0,
           "functions_approx": 0, "branch_tokens": 0,
           "max_file_cyclomatic": 0, "top5_files": [], "excluded": []}
    if not root.is_dir():
        return out
    per_file = []
    for p in sorted(root.rglob("*")):
        if not p.is_file() or p.suffix.lower() not in EXTS:
            continue
        relp = p.relative_to(base).as_posix()
        if any(relp.startswith(ex) or f"/{ex}/" in relp for ex in EXCLUDE_PARTS):
            out["excluded"].append(relp)
            continue
        try:
            text = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        lines = text.count("\n") + (0 if text.endswith("\n") else 1)
        branch = len(BRANCH_RE.findall(text))
        out["files"] += 1
        out["lines"] += lines
        out["functions_approx"] += sum(
            1 for line in text.splitlines() if FUNC_RE.match(line))
        out["branch_tokens"] += branch
        per_file.append((relp, 1 + branch))
    per_file.sort(key=lambda e: -e[1])
    out["max_file_cyclomatic"] = per_file[0][1] if per_file else 0
    out["top5_files"] = [[p, c] for p, c in per_file[:5]]
    return out


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        description="C/C++ 复杂度基线测量（占位实现，只记录不判阈值）")
    ap.add_argument("--paths", default="lib,include,cli",
                    help="逗号分隔的仓库相对目录（默认 lib,include,cli）")
    ap.add_argument("--output", default=None,
                    help="基线报告 JSON 输出路径（仓库相对，如 run/ci/cx/complexity.json）")
    args = ap.parse_args(argv)

    targets = [t.strip() for t in args.paths.split(",") if t.strip()]
    per_path = [scan_path(REPO, t) for t in targets]
    report = {
        "check": "CX-DEEP",
        "generated_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "placeholder": True,
        "ownership": "SA-CI-32",
        "note": ("占位基线测量器：正则近似函数计数，阈值未冻结（合同允许第一次 "
                 "deep CI 只测量记录）；正式 C++ 解析器与阈值冻结由后续质量任务实现。"),
        "threshold": None,
        "totals": {
            "files": sum(t["files"] for t in per_path),
            "lines": sum(t["lines"] for t in per_path),
            "functions_approx": sum(t["functions_approx"] for t in per_path),
            "branch_tokens": sum(t["branch_tokens"] for t in per_path),
            "max_file_cyclomatic": max(t["max_file_cyclomatic"] for t in per_path),
        },
        "excluded_total": sum(len(t["excluded"]) for t in per_path),
        "per_path": per_path,
    }
    payload = json.dumps(report, ensure_ascii=False, indent=2)
    if args.output:
        dst = Path(args.output)
        if not dst.is_absolute():
            dst = REPO / dst
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_text(payload + "\n", encoding="utf-8")
    print(payload)
    return 0


if __name__ == "__main__":
    sys.exit(main())
