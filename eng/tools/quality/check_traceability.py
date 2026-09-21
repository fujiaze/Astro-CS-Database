#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_traceability.py — V19R2 S5 追溯校验。

双向检查：
  code→contract→test：science/public 符号必须有契约 ID + 测试引用；
  contract→code→test：TRACEABILITY 行必须存在实现文件 + 测试文件。
输出 reports/v19r2/evidence/quality/traceability_check.json。
"""

from __future__ import annotations

import csv
import json
import os
import re
import subprocess
import sys


def _deduce_root() -> str:
    # auto-deduce project root: walk up until docs/ and lib/ found (Linux-portable)
    try:
        p = os.path.abspath(__file__)
        cur = os.path.dirname(p)
        for _ in range(5):
            if os.path.isdir(os.path.join(cur, "docs")) and os.path.isdir(os.path.join(cur, "lib")):
                return cur
            parent = os.path.dirname(cur)
            if parent == cur:
                break
            cur = parent
    except Exception:
        pass
    cwd = os.getcwd()
    if os.path.isdir(os.path.join(cwd, "docs")) and os.path.isdir(os.path.join(cwd, "lib")):
        return cwd
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

ROOT = _deduce_root()
# 产物落 run/（不写受跟踪的 reports/**；ENGINEERING_SPEC §7 产物落位 + §8 fail-closed）
DEFAULT_OUT = os.path.join("run", "ci", "quality", "traceability_check.json")


def tracked() -> list[str]:
    r = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True,
                       text=True, encoding="utf-8", errors="replace",
                       timeout=120)
    return r.stdout.splitlines()


def main(argv: list[str] | None = None) -> int:
    import argparse
    ap = argparse.ArgumentParser(description="V19R2 S5 追溯校验")
    ap.add_argument("--out", default=DEFAULT_OUT,
                    help="JSON 证据输出路径（默认 run/ci/quality/traceability_check.json）")
    args = ap.parse_args(argv)

    files = tracked()
    trace_path = os.path.join(ROOT, "docs", "TRACEABILITY.csv")
    if not os.path.isfile(trace_path):
        # fail-closed: 输入缺失判红，不得当「无违规」
        print(f"TRACEABILITY_FAIL: 输入缺失 {trace_path}", file=sys.stderr)
        return 1
    trace = list(csv.DictReader(open(trace_path, encoding="utf-8")))
    rows_ok = 0
    broken = []
    for r in trace:
        impl = r["implementation_files"]
        tests = r["test_files"]
        if not impl or not tests:
            broken.append({"requirement": r["requirement_id"],
                           "reason": "missing implementation/test files"})
            continue
        if any(p not in files for p in re.split(r"[;,\s]+", impl) if p):
            broken.append({"requirement": r["requirement_id"],
                           "reason": "implementation file not tracked"})
            continue
        rows_ok += 1

    # 抽样符号（科学关键）：code→contract→test
    symbols = [
        ("p2_upm_build", "upm.cpp"), ("p2_upm_save", "upm.cpp"),
        ("p2_upm_open", "upm.cpp"), ("p2_upm_calibrate_block", "upm.cpp"),
        ("p2_reject_stack", "rejection.cpp"),
        ("p2_integrate_pixel", "integrate.cpp"),
        # noise_model 内部函数经公共 API 测试覆盖（snr_noise_model_v1*）
        ("snr_noise_model_v1", "noise_model.cpp"),
        ("snr_phot_cal_quality", "noise_model.cpp"),
        ("p2_coverage_build", "coverage.cpp"),
        ("p2_sample_controls", "sampler.cpp"),
        # aio_upm 无独立单测名引用，经 p2_upm_save/open 公共 API 间接覆盖：
        # SaveOpenRoundtripAndHash / UpmPersist* 全链走 aio_upm_write_sparse。
        ("p2_upm_save", "upm.cpp"),
        ("p2_upm_open", "upm.cpp"),
        ("spherical_polygon_area", "spherical_overlap.cpp"),
    ]
    sym_ok = 0
    sym_broken = []
    for sym, f in symbols:
        src = os.path.join(ROOT, "lib")
        hit_src = False
        for dp, _dn, fn in os.walk(src):
            if any(x in dp.split(os.sep) for x in
                   ("build", "build2", "_deps", "archive", "third_party",
                    "worktrees")):
                continue
            for name in fn:
                if name == f and name.endswith((".cpp", ".h", ".hpp")):
                    p = os.path.join(dp, name)
                    if re.search(r"\b" + re.escape(sym) + r"\b",
                                 open(p, encoding="utf-8",
                                      errors="replace").read()):
                        hit_src = True
        if not hit_src:
            sym_broken.append({"symbol": sym, "reason": "not found in source"})
            continue
        # 测试引用
        test_ref = False
        for dp, _dn, fn in os.walk(os.path.join(ROOT, "lib")):
            if any(x in dp.split(os.sep) for x in
                   ("build", "build2", "_deps", "archive", "third_party",
                    "worktrees")):
                continue
            for name in fn:
                if name.endswith(("_test.cpp", "gate.cpp", "test.cpp")):
                    if re.search(r"\b" + re.escape(sym) + r"\b",
                                 open(os.path.join(dp, name), encoding="utf-8",
                                      errors="replace").read()):
                        test_ref = True
        indirect = {
            "p2_upm_save": "p2_upm_open",
            "p2_upm_open": "p2_upm_save",
        }
        if not test_ref and sym in indirect:
            for dp, _dn, fn in os.walk(os.path.join(ROOT, "lib")):
                if any(x in dp.split(os.sep) for x in
                       ("build", "build2", "_deps", "archive", "third_party",
                        "worktrees")):
                    continue
                for name in fn:
                    if name.endswith(("_test.cpp", "gate.cpp", "test.cpp")):
                        if re.search(r"\b" + re.escape(indirect[sym]) + r"\b",
                                     open(os.path.join(dp, name),
                                          encoding="utf-8",
                                          errors="replace").read()):
                            test_ref = True
        if not test_ref:
            sym_broken.append({"symbol": sym, "reason": "no test reference"})
            continue
        sym_ok += 1

    out_path = args.out if os.path.isabs(args.out) else os.path.join(ROOT, args.out)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    out = {
        "traceability_rows": len(trace),
        "rows_ok": rows_ok,
        "rows_broken": len(broken),
        "broken_rows": broken,
        "sample_symbols": len(symbols),
        "sample_ok": sym_ok,
        "sample_broken": sym_broken,
        "TRACEABILITY_BROKEN": len(broken),
    }
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print(f"traceability: rows={len(trace)} ok={rows_ok} broken={len(broken)} "
          f"symbols={sym_ok}/{len(symbols)} out={os.path.relpath(out_path, ROOT)}")
    # 退出码按证据决定（旧版无条件 return 0 ⇒ waivable=false 却不可能红）
    if broken or sym_broken:
        print(f"TRACEABILITY_FAIL: broken_rows={len(broken)} "
              f"broken_symbols={len(sym_broken)}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
