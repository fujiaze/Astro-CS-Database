#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-ROOT-CLEAN：仓库根目录整洁机器门（ROOT-002 产物）。

权威：
  * ENGINEERING_SPEC.md §7 —— 仓库根固定条目白名单；「任何新产物必须落位到
    对应目录，禁止散落根目录」「确需新增根目录条目，先登记并获得负责人确认」；
  * ASTROCS_DESIGN.md §6.3 —— 运行产物只落配置 output_dir，不得以进程 CWD
    作隐式缺省写出；
  * eng/ci/root_manifest.json —— §7 白名单的机器可读落盘（逐条对照，禁止更宽）。

判红（rc=1）的三类：
  1. 顶层条目 ∉ §7 白名单、且未被登记为本地保留/容忍（未登记条目落根）；
  2. 根目录出现 astrocs_run_* 等运行产物（无论是否被 .gitignore 覆盖）；
  3. §7 要求存在的条目缺失。

能绿能红：见 eng/tests/quality/test_root_cleanliness.py（1 正例 + 3 负例）。
输出：机器 JSON（stdout + 可选 --json-out）。
"""
from __future__ import annotations

import argparse
import datetime
import fnmatch
import json
import os
import pathlib
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent.parent.parent
DEFAULT_MANIFEST = REPO / "eng" / "ci" / "root_manifest.json"


def _load_manifest(path: pathlib.Path) -> dict:
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def _norm(pattern: str) -> str:
    return pattern.rstrip("/")


def _matches(name: str, patterns) -> bool:
    return any(fnmatch.fnmatchcase(name, _norm(p)) for p in patterns or [])


def check(root: pathlib.Path, manifest: dict) -> dict:
    allowed_files = set(manifest.get("allowed_files", []))
    allowed_dirs = set(manifest.get("allowed_dirs", []))
    implicit = set(manifest.get("implicit_entries", []))
    required_files = manifest.get("required_files", [])
    required_dirs = manifest.get("required_dirs", [])
    runtime_pats = manifest.get("runtime_product_patterns", [])
    ignored_pats = manifest.get("ignored_patterns", [])
    registered = manifest.get("registered_local_retention", [])
    reg_by_path = {r["path"]: r for r in registered}

    entries = sorted(os.listdir(root)) if root.is_dir() else []

    violations: list[dict] = []
    allowed_present: list[str] = []
    implicit_present: list[str] = []
    registered_present: list[dict] = []
    tolerated_present: list[dict] = []

    for name in entries:
        if name in allowed_files or name in allowed_dirs:
            allowed_present.append(name)
            continue
        if name in implicit:
            implicit_present.append(name)
            continue
        if _matches(name, runtime_pats):
            violations.append({
                "path": name, "reason": "runtime_product_at_root",
                "detail": "运行产物落根（ASTROCS_DESIGN §6.3 / ENGINEERING_SPEC §7 禁止；"
                          "CLI 产物只落 output_dir）",
            })
            continue
        if name in reg_by_path:
            registered_present.append({"path": name, **reg_by_path[name]})
            continue
        if _matches(name, ignored_pats):
            tolerated_present.append({
                "path": name, "reason": "gitignored_local_dir",
                "detail": "被 .gitignore 覆盖的本地目录，记容忍并上报（非白名单扩展）",
            })
            continue
        violations.append({
            "path": name, "reason": "unregistered_root_entry",
            "detail": "顶层条目不在 ENGINEERING_SPEC §7 白名单，且未登记为本地保留；"
                      "新增根条目须先登记并经负责人确认",
        })

    missing: list[dict] = []
    for name in required_files:
        p = root / name
        if not p.is_file():
            missing.append({"path": name, "reason": "missing_required_file"})
    for name in required_dirs:
        p = root / name
        if not p.is_dir():
            missing.append({"path": name, "reason": "missing_required_dir"})
    violations.extend(missing)

    verdict = "PASS" if not violations else "FAIL"
    return {
        "check": "CHK-ROOT-CLEAN",
        "verdict": verdict,
        "root": str(root),
        "manifest": str(DEFAULT_MANIFEST),
        "generated_at": datetime.datetime.now().astimezone().isoformat(timespec="seconds"),
        "counts": {
            "top_level_entries": len(entries),
            "allowed": len(allowed_present),
            "implicit": len(implicit_present),
            "registered_pending_owner": len(registered_present),
            "tolerated_ignored": len(tolerated_present),
            "violations": len(violations),
        },
        "violations": violations,
        "registered_pending_owner": registered_present,
        "tolerated_ignored": tolerated_present,
        "allowed_present": allowed_present,
        "implicit_present": implicit_present,
    }


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="CHK-ROOT-CLEAN 仓库根目录整洁检查")
    ap.add_argument("--root", default=str(REPO), help="被检查的根目录（默认仓库根；测试可用临时树）")
    ap.add_argument("--manifest", default=str(DEFAULT_MANIFEST), help="eng/ci/root_manifest.json 路径")
    ap.add_argument("--json-out", default=None, help="机器 JSON 落盘路径（如 run/ci/root-cleanliness/root_cleanliness.json）")
    ap.add_argument("--quiet", action="store_true", help="不向 stdout 打印完整 JSON（仅打印 verdict 摘要）")
    args = ap.parse_args(argv)

    manifest = _load_manifest(pathlib.Path(args.manifest))
    report = check(pathlib.Path(args.root), manifest)
    report["manifest"] = str(pathlib.Path(args.manifest))

    text = json.dumps(report, ensure_ascii=False, indent=2)
    if args.json_out:
        out = pathlib.Path(args.json_out)
        if not out.is_absolute():
            out = REPO / out
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text + "\n", encoding="utf-8")
    if args.quiet:
        print(json.dumps({"check": report["check"], "verdict": report["verdict"],
                          "counts": report["counts"]}, ensure_ascii=False))
    else:
        print(text)
    return 0 if report["verdict"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
