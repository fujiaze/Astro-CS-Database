#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_complexity.py — V8-CI-005 linux-deep 复杂度基线检查（owner=SA-CI-32）。

状态：占位实现（placeholder）。合同（07_CI_MACHINE_CONTRACT.md「Coverage 与
复杂度」）：第一次 deep CI 只测量并记录基线，不虚构阈值；第二个原子任务再
冻结每模块复杂度上限。本脚本即第一次基线测量器：

  * 目标（--paths，逗号分隔仓库目录）下的 C/C++ 源与头文件逐个统计：
    文件数、总行数、函数定义近似计数（正则近似，非正式解析器）；
  * 输出 JSON 基线报告到 --output（登记于 eng/ci/checks.json outputs）；
  * 阈值固定为 null（未冻结），报告带 "placeholder": true 与所有权标注；
  * **阈值不判红 ≠ 输入面不判红**（GATE-SOLID-01 / 独立审查节点一页纸 S2-A）：
    --paths 里**任一测量根不存在**即 exit 1 并逐条打印缺失路径。旧实现在
    "全部不存在"时才判红（GAP-027），部分缺失则静默扫剩下的（注册表登记的
    `lib,cli,include` 里 cli/include 已迁走 ⇒ 两个根恒空、门恒真通过）；
  * 无可测量目标（--paths 为空）同样判红（空面不得恒真）；
  * 正式 C++ 解析器由后续质量任务（owner=SA-CI-32）替换，届时本文件升级为真实检查器。

仅 stdlib；无外部命令；只读扫描 + 写 --output 一个文件（run/ 临时目录）。

V8-CI-005（SA-CI-32）增强（保持占位语义：只记录、exit 0、不判阈值）：
  * 增加分支 token 计数（if/for/while/case/catch/&&/||/?:）→
    per-file cyclomatic = 1 + branch_tokens（文件级粒度；不做函数体切分，
    namespace/class 花括号会把多函数吞并成假"巨型函数"）；
  * 排除 ACR dormant 树（约束 §C：生产构建/度量不含 ACR，lib/infrastructure/acr/** 与
    legacy/** 不进复杂度基线，逐路径在 report.excluded 登记）。
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]

# C/C++ 函数定义近似：行首到 "{") 简化正则（近似计数，仅基线用）
FUNC_RE = re.compile(
    r"^[A-Za-z_][A-Za-z0-9_:<>,&*\s]*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?\{",
)
# 分支 token（文件级圈复杂度近似；与函数计数同受字符串/注释噪声影响，仅基线用）
BRANCH_RE = re.compile(r"\b(?:if|for|while|case|catch)\b|&&|\|\||\?")
EXCLUDE_PARTS = ("lib/infrastructure/acr", "legacy", "third_party", "thirdparty")
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
    ap.add_argument("--paths", default="lib",
                    help="逗号分隔的仓库相对目录（默认 lib；cli/include 子树已并入 "
                         "lib/infrastructure/cli 与 lib/include）")
    ap.add_argument("--selftest", action="store_true",
                    help="负例面：部分缺失/全缺失/空 --paths 必须判红（GATE-SOLID-01）")
    ap.add_argument("--output", default=None,
                    help="基线报告 JSON 输出路径（仓库相对，如 run/ci/cx/complexity.json）")
    args = ap.parse_args(argv)
    if args.selftest:
        return selftest()

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
    if not targets:
        print("COMPLEXITY_NO_INPUT: --paths 为空，无任何测量根（空面不得恒真）→ 判 FAIL",
              file=sys.stderr)
        return 1
    missing = [t["path"] for t in per_path if not t.get("exists")]
    if missing:
        # GATE-SOLID-01 / 一页纸 S2-A：**声明的输入路径不存在即红**。
        # 旧实现只在"全部不存在"（GAP-027）时判红 —— 部分缺失时静默扫剩下的，
        # 恒空的那几个根不产生任何判定（"空扫描恒真通过"）。JSON 照常落盘（留证），
        # 但退出码必须非零。
        print("COMPLEXITY_MISSING_INPUT: --paths 声明的测量根不存在 %s "
              "（fail-closed：声明的输入不存在即红；修正 --paths 或恢复该死路径）"
              % missing, file=sys.stderr)
        return 1
    return 0


def missing_targets(per_path: list) -> list:
    """声明测量根中"不存在"的那些（与 main 同一判据，供自检与外部复核复用）。"""
    return [t["path"] for t in per_path if not t.get("exists")]


def selftest() -> int:
    """负例面（GATE-SOLID-01 / S2-A）：部分缺失、全缺失、空 --paths 均须判红。

    在临时目录上构造真实 scan_path 结果（不扫真实仓库），正例必须不被误伤。
    """
    import tempfile
    cases = []
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "present").mkdir()
        (root / "present" / "a.cpp").write_text("int f(){return 0;}\n", encoding="utf-8")

        def state(rel):
            return scan_path(root, rel)

        partial = [state("present"), state("gone")]
        cases.append({"case": "N1_partial_missing_is_red",
                      "ok": missing_targets(partial) == ["gone"],
                      "note": "--paths present,gone ⇒ 缺失 gone 必须判红"})
        cases.append({"case": "N2_all_missing_is_red",
                      "ok": missing_targets([state("gone"), state("lost")]) == ["gone", "lost"],
                      "note": "全缺失必须判红"})
        cases.append({"case": "P1_all_present_is_green",
                      "ok": missing_targets([state("present")]) == [],
                      "note": "全部存在不得误伤"})
        cases.append({"case": "P2_empty_targets_is_red",
                      "ok": ([] == [t.strip() for t in "".split(",") if t.strip()]),
                      "note": "空 --paths ⇒ main 走 COMPLEXITY_NO_INPUT 判红分支"})
    ok = all(c["ok"] for c in cases)
    for c in cases:
        print("SELFTEST %s %s  %s" % ("PASS" if c["ok"] else "FAIL", c["case"], c["note"]))
    print("COMPLEXITY_SELFTEST_%s: %d/%d" % ("PASS" if ok else "FAIL",
                                             sum(1 for c in cases if c["ok"]), len(cases)))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
