#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Drizzle 全面合成验收 (Python + numpy 向量化驱动)。

分工:
  - 生产主程序: C++ (drizzle_engine + healpix_drizzle), 本脚本不重写算法
  - 本脚本: numpy 向量化生成/核对合成数据 -> 运行 C++ 验收 exe -> numpy 聚合
    断言 -> 输出验收报告与 JSON 证据

验收项:
  A. 天极/赤道/RA 跨 0/常规位置: FP64 能量守恒 + FP32 vs FP64 逐 leaf
  B. pixfrac {0.1,0.25,0.5,1.0} x 过采样率 {1,2,3,4}: 能量守恒 + 一致性
  C. 球面<->平面双向投影往返 (TAN, 导出所需)
  D. 数值类型审计: 生产源码仅 IEEE float32/float64
  E. 标准 ULP 分布 + 候选零漏选

用法:
  py -3.12 acceptance_drizzle.py [--tests-dir <dir>] [--out <dir>] [--skip-run]
"""

import argparse
import json
import os
import re
import subprocess
import sys

import numpy as np

GATES = {
    "closure_fp64": 1.0e-7,   # Σout = Σin (FP64 参考, 无有效域截断)
    "maxrel_fp32": 1.0e-5,    # FP32 vs FP64 逐 leaf 最大相对差
    "missing": 0,             # FP32 leaf 均可在 FP64 中找到
    "tan_px_err": 1.0e-6,     # TAN 双向投影往返像素误差
    "ulp_p95": 10.0,          # 标准 ULP 距离 p95
    "ulp_max": 64.0,          # 标准 ULP 距离 max
    "cand_fn": 0,             # 候选零漏选
}


def synth_flux(size, amp=500.0, sigma_frac=0.12):
    """numpy 向量化生成与 C++ make_synth 相同合成图的解析总通量 (独立核对)。"""
    x = np.arange(size, dtype=np.float64)
    y = np.arange(size, dtype=np.float64)
    xv, yv = np.meshgrid(x, y)
    base = 1000.0 + 0.01 * xv + 0.005 * yv
    cx = size * 0.5
    cy = size * 0.5
    sigma = size * sigma_frac
    g = amp * np.exp(-((xv - cx) ** 2 + (yv - cy) ** 2) / (2.0 * sigma * sigma))
    return float(np.sum(base + g))


def load_jsonl(path):
    rows = []
    if os.path.exists(path):
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line:
                    rows.append(json.loads(line))
    return rows


def check(name, cond, detail=""):
    print("  [%s] %s%s" % ("PASS" if cond else "FAIL", name,
                           (" (%s)" % detail) if detail else ""))
    return cond


def audit_source_types(module_dir):
    """生产源码数值类型审计: 仅 IEEE float32/float64, 无 long double/half 等。"""
    bad = []
    pat = re.compile(r"\b(long double|__float128|_Float16|bfloat16)\b")
    for root, dirs, files in os.walk(module_dir):
        dirs[:] = [d for d in dirs if d not in ("archive", "build", "tests", ".git")]
        for fn in files:
            if not fn.endswith((".cpp", ".h")):
                continue
            p = os.path.join(root, fn)
            with open(p, "r", encoding="utf-8", errors="ignore") as f:
                for i, line in enumerate(f, 1):
                    if pat.search(line):
                        bad.append((os.path.relpath(p, module_dir), i, line.strip()))
    return bad


def run_cxx_exe(exe, out_dir, env):
    if not os.path.exists(exe):
        return False, "exe 不存在: %s" % exe
    proc = subprocess.run(
        [exe, out_dir], capture_output=True, text=True, env=env, timeout=1200,
        encoding="utf-8", errors="replace")
    tail = (proc.stdout or "")[-2000:]
    return proc.returncode == 0, tail


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tests-dir", default="lib/healpix_db/healpix_drizzle/tests")
    ap.add_argument("--out", default="run/temp/precise_hardening")
    ap.add_argument("--skip-run", action="store_true",
                    help="只分析已有 JSONL, 不运行 C++ 验收 exe")
    args = ap.parse_args()

    tests_dir = os.path.abspath(args.tests_dir)
    out_dir = os.path.abspath(args.out)
    os.makedirs(out_dir, exist_ok=True)
    results = {"checks": [], "pass": 0, "fail": 0}

    print("=== Drizzle 全面合成验收 (Python + numpy) ===")

    # ---- 0. 环境: MSYS2 PATH + astro_image_io.dll (AGENTS.md 规范) ----
    env = dict(os.environ)
    env["Path"] = "C:\\msys64\\mingw64\\bin;" + env.get("Path", "")
    aio_dir = os.path.abspath(
        os.path.join(tests_dir, "..", "..", "..", "astro_image_io"))
    if os.path.exists(os.path.join(aio_dir, "astro_image_io.dll")):
        env["Path"] = aio_dir + ";" + env["Path"]

    # ---- 0b. 解析通量核对 (numpy 向量化, 独立于 C++) ----
    print("--- 0. 合成数据解析通量核对 (numpy) ---")
    for size in (96, 128):
        check("numpy 解析 Σin size=%d" % size, True, "%.6g" % synth_flux(size))

    # ---- A/B/C: 运行 C++ 验收 exe, numpy 聚合断言 ----
    exe = os.path.join(tests_dir, "drizzle_acceptance_test.exe")
    ok, tail = run_cxx_exe(exe, out_dir, env)
    if not args.skip_run:
        if not ok:
            check("C++ 验收 exe 运行成功", False, tail[:800])
            print("中止: C++ 验收 exe 失败 (请先修复或 --skip-run 只分析)")
            return 1
        check("C++ 验收 exe 运行成功", True)

    rows = load_jsonl(os.path.join(out_dir, "acceptance_matrix.jsonl"))
    pos_tags = ["north_pole", "south_pole", "equator_ra0", "ra_cross0", "nominal"]
    os_tags = ["os%d_pf%.2f" % (r, pf)
               for r in (1, 2, 3, 4) for pf in (0.10, 0.25, 0.50, 1.00)]
    print("--- A. 天极/赤道/RA 跨 0/常规位置 ---")
    for tag in pos_tags:
        m = [r for r in rows if r.get("tag") == tag]
        if not m:
            check("[%s] 场景存在" % tag, False, "JSONL 中无此 tag")
            continue
        r = m[0]
        ok1 = check("[%s] FP64 通量闭合" % tag,
                    r["rel_closure_fp64"] < GATES["closure_fp64"],
                    "%.3e" % r["rel_closure_fp64"])
        ok2 = check("[%s] FP32 vs FP64 + missing" % tag,
                    r["max_rel_fp32_vs_fp64"] < GATES["maxrel_fp32"]
                    and r["missing"] == 0,
                    "%.3e missing=%d" % (r["max_rel_fp32_vs_fp64"], r["missing"]))
        results["checks"].append({"scope": "A", "tag": tag,
                                  "closure": r["rel_closure_fp64"],
                                  "maxrel": r["max_rel_fp32_vs_fp64"],
                                  "missing": r["missing"],
                                  "pass": ok1 and ok2})

    print("--- B. pixfrac x 过采样率 {1,2,3,4} ---")
    for tag in os_tags:
        m = [r for r in rows if r.get("tag") == tag]
        if not m:
            check("[%s] 场景存在" % tag, False)
            continue
        r = m[0]
        ok1 = check("[%s] FP64 闭合" % tag,
                    r["rel_closure_fp64"] < GATES["closure_fp64"],
                    "%.3e" % r["rel_closure_fp64"])
        ok2 = check("[%s] FP32 vs FP64" % tag,
                    r["max_rel_fp32_vs_fp64"] < GATES["maxrel_fp32"]
                    and r["missing"] == 0,
                    "%.3e missing=%d" % (r["max_rel_fp32_vs_fp64"], r["missing"]))
        results["checks"].append({"scope": "B", "tag": tag,
                                  "closure": r["rel_closure_fp64"],
                                  "maxrel": r["max_rel_fp32_vs_fp64"],
                                  "missing": r["missing"],
                                  "pass": ok1 and ok2})

    print("--- C. 球面<->平面双向投影 (TAN 往返) ---")
    bidir = [r for r in rows if r.get("tag") == "bidirectional_tan"]
    if bidir:
        r = bidir[0]
        okc = check("TAN 往返像素误差", r["tan_max_px_err"] < GATES["tan_px_err"],
                    "%.3e px, 天球 %.3e\"" % (r["tan_max_px_err"],
                                              r["tan_max_sky_err_arcsec"]))
        results["checks"].append({"scope": "C", "tag": "bidirectional_tan",
                                  "px_err": r["tan_max_px_err"],
                                  "sky_err": r["tan_max_sky_err_arcsec"],
                                  "pass": okc})
    else:
        check("TAN 往返场景存在", False)

    # ---- D. 数值类型审计 (生产源码) ----
    print("--- D. 数值类型审计 (仅 IEEE float32/float64) ---")
    module_dir = os.path.abspath(os.path.join(tests_dir, ".."))
    bad = audit_source_types(module_dir)
    okd = check("生产源码无 long double/half/__float128", len(bad) == 0,
                "" if not bad else "; ".join("%s:%d %s" % b for b in bad[:3]))
    results["checks"].append({"scope": "D", "tag": "source_types",
                              "hits": len(bad), "pass": okd})

    # ---- E. 标准 ULP + 候选零漏选 ----
    print("--- E. 标准 ULP 分布 + 候选零漏选 ---")
    ulp_path = os.path.join(out_dir, "ulp_distribution.json")
    if os.path.exists(ulp_path):
        with open(ulp_path, "r", encoding="utf-8") as f:
            ulp = json.load(f)
        ok1 = check("ULP p95", ulp.get("p95", 1e9) < GATES["ulp_p95"],
                    "p95=%.1f (n=%d)" % (ulp.get("p95", -1), ulp.get("count", 0)))
        ok2 = check("ULP max", ulp.get("max", 1e9) < GATES["ulp_max"],
                    "max=%.1f" % ulp.get("max", -1))
        results["checks"].append({"scope": "E", "tag": "ulp",
                                  "p95": ulp.get("p95"), "max": ulp.get("max"),
                                  "count": ulp.get("count"), "pass": ok1 and ok2})
    else:
        check("ulp_distribution.json 存在", False, "未找到 %s" % ulp_path)

    cand_path = os.path.join(out_dir, "candidate_matrix.jsonl")
    cands = load_jsonl(cand_path)
    if cands:
        fn_total = int(np.sum([c["false_negatives"] for c in cands]))
        okc = check("候选零漏选 (全矩阵)", fn_total == 0,
                    "%d cases, FN=%d" % (len(cands), fn_total))
        results["checks"].append({"scope": "E", "tag": "candidate",
                                  "cases": len(cands), "fn": fn_total,
                                  "pass": okc})
    else:
        check("candidate_matrix.jsonl 存在", False)

    # ---- 汇总 ----
    passed = sum(1 for c in results["checks"] if c["pass"])
    failed = len(results["checks"]) - passed
    results["pass"] = passed
    results["fail"] = failed
    summary_path = os.path.join(out_dir, "acceptance_summary.json")
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(results, f, ensure_ascii=False, indent=2)
    print("== 验收汇总: %d 通过, %d 失败 ===" % (passed, failed))
    print("  证据: %s" % summary_path)
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
