#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Drizzle 全面合成验收 (Python + numpy 向量化驱动)。

分工:
  - 生产主程序: C++ (drizzle_engine + healpix_drizzle), 本脚本不重写算法
  - 本脚本: numpy 向量化生成/核对合成数据 -> 运行 C++ 验收 exe -> numpy 聚合
    断言 -> 输出验收报告与 JSON 证据

验收项 (DRZ-FLUX-FIX-01: 核按 drop 面积归一 ⇒ Sigma_out = Sigma_in, 与 pixfrac 无关):
  0.  环境 (exe 可定位 / exe 运行成功) 与 numpy 解析通量核对
     0b 只是「打印解析通量」的诊断项, 判定值为字面 True、不构成门;
     它进汇总只为让摘要与打印逐条对齐, 不得当作证据读。
  A. 天极/赤道/RA 跨 0/常规位置: FP64 能量守恒 + FP32 vs FP64 逐 leaf
  B. pixfrac {0.1,0.5,0.8,1.0} x 过采样率 {1,2,3,4}: 能量守恒 + 一致性
  C. 球面<->平面双向投影往返 (TAN, 导出所需)
  D. 尺度 x NSIDE 矩阵 (0.5"/1"/2"/3" + 对应 NSIDE)
  E. 广域大畸变矩阵 (T4 真实 WCS + 合成 SIP5 + 极区 + RA 跨 0)
  F. 数值类型审计: 生产源码仅 IEEE float32/float64
  G. 标准 ULP 分布 + 候选零漏选
  H. 负例控制: 同一 exe 注入旧 pixfrac^2 口径 (--inject-legacy-pixfrac2) 必须判红

判定记账 (硬约束, 见 finalize):
  每一条被打印的判定都必须进入 acceptance_summary.json 的 checks 容器,
  pass/fail 只从该容器算出, 不存在「打印但不计入」的旁路。实现上 check() 是
  打印与记账的唯一通道; finalize() 的漏计守卫独立地数本次 stdout, 打印条数或
  FAIL 数与记账数不等即判红并返回非 0 退出码。任何代码出口都必经 finalize(),
  因此不存在「提前 return 而留下上一轮的绿色摘要」。

用法:
  python3 acceptance_drizzle.py [--exe <path>] [--tests-dir <dir>] [--out <dir>] [--skip-run]

exe 定位 (跨平台): --exe 显式给定 > 环境 ACSD_DRIZZLE_ACCEPTANCE_EXE >
tests_dir / build 下若干候选 (含/不含 .exe 后缀, 递归 glob)。
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

# 判定行行首的唯一来源: print 与漏计守卫共用, 改这里必须同步改两个正则。
VERDICT_HEAD = "  [%s] "
VERDICT_FMT = VERDICT_HEAD + "%s%s"
VERDICT_RE_PASS = re.compile("(?m)^" + re.escape(VERDICT_HEAD % "PASS"))
VERDICT_RE_FAIL = re.compile("(?m)^" + re.escape(VERDICT_HEAD % "FAIL"))


class _Tee(object):
    """把真实 stdout 复制一份留底, 供漏计守卫独立复核「本次到底打印了什么」。

    守卫必须走一条与记账无关的路才可能发现漏计, 所以它读 tee.text (真实输出),
    不读 Ledger 自己攒的列表。
    """

    def __init__(self, stream):
        self.stream = stream
        self._parts = []

    def write(self, s):
        self._parts.append(s)
        return self.stream.write(s)

    def flush(self):
        return self.stream.flush()

    def isatty(self):
        return getattr(self.stream, "isatty", lambda: False)()

    @property
    def text(self):
        return "".join(self._parts)


def one_line(text):
    """把 detail 压成单行: 一条判定必须只占一个物理行。

    C++ 验收 exe 自己也用「  [FAIL] 」开头打印失败行 (drizzle_acceptance_test.cpp:39/:91),
    而 exe 的输出会作为 detail 内嵌进来。不压成单行时, 内嵌的 exe 失败行会被守卫
    误认成本脚本的判定行。
    """
    return " ⏎ ".join(str(text).split("\n"))


class Ledger(object):
    """判定账本: 打印与记账的唯一通道。

    record() 打印一条判定, 同时把同一条判定写进 results["checks"]。判定不可能
    只打印不记账 —— 这是「汇总漏计」这一类缺陷的结构性消除, 而不是逐点补丁。
    """

    def __init__(self, sink):
        self.sink = sink          # results["checks"]

    def record(self, name, cond, detail="", scope="-", tag=None, **metrics):
        cond = bool(cond)
        print(VERDICT_FMT % ("PASS" if cond else "FAIL", name,
                             (" (%s)" % one_line(detail)) if detail else ""))
        entry = {"scope": scope, "tag": tag if tag is not None else name,
                 "name": name, "pass": cond}
        if detail:
            entry["detail"] = one_line(detail)
        entry.update(metrics)
        self.sink.append(entry)
        return cond


_LEDGER = None


def check(name, cond, detail="", scope="-", tag=None, **metrics):
    """打印一条判定并同时记账; 返回 cond, 供调用点聚合用。

    只允许在 main() 建立账本后调用, 否则是编程错误 —— 静默忽略会让判定消失。
    """
    if _LEDGER is None:
        raise RuntimeError("判定账本未建立: check() 只能在 main() 内调用")
    return _LEDGER.record(name, cond, detail, scope, tag, **metrics)


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


def find_exe(explicit, tests_dir):
    """跨平台定位 drizzle_acceptance_test。

    历史缺陷: 原实现硬编码 tests_dir/drizzle_acceptance_test.exe (Windows 专名),
    在 Linux 上恒为 "exe 不存在"——验收门永远跑不起来。
    搜索顺序: --exe > $ACSD_DRIZZLE_ACCEPTANCE_EXE > tests_dir 及其上溯的
    build/ 树下若干候选 (带/不带 .exe 后缀, 含递归 glob)。
    """
    if explicit:
        return explicit if os.path.exists(explicit) else None
    env_exe = os.environ.get("ACSD_DRIZZLE_ACCEPTANCE_EXE")
    if env_exe and os.path.exists(env_exe):
        return env_exe
    names = ("drizzle_acceptance_test", "drizzle_acceptance_test.exe")
    roots = [tests_dir]
    here = tests_dir
    for _ in range(6):
        here = os.path.dirname(here)
        roots.append(os.path.join(here, "build"))
    for root in roots:
        if not os.path.isdir(root):
            continue
        for name in names:
            cand = os.path.join(root, name)
            if os.path.isfile(cand):
                return cand
    # 最后回退: 在 build/ 下递归找 (target 输出目录布局可能变化)
    import glob as _glob
    for root in roots:
        if not os.path.isdir(root):
            continue
        for name in names:
            hits = sorted(_glob.glob(os.path.join(root, "**", name),
                                     recursive=True))
            if hits:
                return hits[0]
    return None


def run_cxx_exe(exe, out_dir, env, extra_args=()):
    if not os.path.exists(exe):
        return False, "exe 不存在: %s" % exe
    proc = subprocess.run(
        [exe, out_dir] + list(extra_args), capture_output=True, text=True,
        env=env, timeout=3600, encoding="utf-8", errors="replace")
    tail = (proc.stdout or "")[-2000:]
    return proc.returncode == 0, tail


def finalize(results, ledger, tee, out_dir):
    """唯一的出口: 从账本算汇总 -> 漏计守卫 -> 落盘 -> 返回退出码。

    漏计守卫 (自证式判据): 独立地数本次实际打印出去的判定行, 与容器里的条数和
    失败数比对。两者不等 ⇒ 存在「打印了却没计入」的漏计 ⇒ 守卫自身判红。
    守卫的判定也走 record() 这同一条通道, 所以事后打印数与记账数仍相等,
    不会因为守卫自己而产生新的不等。

    因为所有代码出口都必经本函数, 提前中止时也会写出一份非绿摘要,
    不会留下上一轮的绿色 acceptance_summary.json 给下游读。
    """
    checks = results["checks"]
    printed_fail = len(VERDICT_RE_FAIL.findall(tee.text))
    printed_total = len(VERDICT_RE_PASS.findall(tee.text)) + printed_fail
    recorded_fail = sum(1 for c in checks if not c["pass"])
    guard_ok = (printed_total == len(checks)) and (printed_fail == recorded_fail)
    results["guard"] = {"printed_verdicts": printed_total,
                        "printed_fail": printed_fail,
                        "recorded_verdicts": len(checks),
                        "recorded_fail": recorded_fail,
                        "consistent": bool(guard_ok)}

    ledger.record("判定账本自洽 (打印条数/失败数与记账一致)", guard_ok,
                  "打印 %d 条(FAIL %d) / 记账 %d 条(fail %d)"
                  % (printed_total, printed_fail, len(checks), recorded_fail),
                  scope="GUARD", tag="ledger_consistency")

    passed = sum(1 for c in checks if c["pass"])
    failed = len(checks) - passed
    results["pass"] = passed
    results["fail"] = failed

    summary_path = os.path.join(out_dir, "acceptance_summary.json")
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(results, f, ensure_ascii=False, indent=2)
    print("== 验收汇总: %d 通过, %d 失败 ===" % (passed, failed))
    print("  证据: %s" % summary_path)
    return 0 if failed == 0 else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tests-dir", default="lib/algorithms/drizzle/healpix_drizzle/tests")
    ap.add_argument("--out", default="run/temp/precise_hardening")
    ap.add_argument("--exe", default=None,
                    help="drizzle_acceptance_test 可执行文件路径 (默认自动定位)")
    ap.add_argument("--skip-run", action="store_true",
                    help="只分析已有 JSONL, 不运行 C++ 验收 exe")
    args = ap.parse_args()

    tests_dir = os.path.abspath(args.tests_dir)
    out_dir = os.path.abspath(args.out)
    os.makedirs(out_dir, exist_ok=True)
    results = {"checks": [], "skipped": [], "pass": 0, "fail": 0}

    global _LEDGER
    ledger = Ledger(results["checks"])
    _LEDGER = ledger
    tee = _Tee(sys.stdout)
    real_stdout = sys.stdout
    sys.stdout = tee
    try:
        _run_checks(args, tests_dir, out_dir, results)
    finally:
        _LEDGER = None
        sys.stdout = real_stdout
    return finalize(results, ledger, tee, out_dir)


def _run_checks(args, tests_dir, out_dir, results):
    """跑全部验收项。遇不可继续的硬失败 (exe 缺失 / exe 运行失败) 即返回,
    汇总与退出码一律交给 finalize() —— 本函数不直接 return 退出码, 因此
    不存在「提前 return 而不写摘要」的出口。"""

    print("=== Drizzle 全面合成验收 (Python + numpy) ===")

    # ---- 0. 环境: MSYS2 PATH + astro_image_io.dll (AGENTS.md 规范) ----
    env = dict(os.environ)
    env["Path"] = "C:\\msys64\\mingw64\\bin;" + env.get("Path", "")
    aio_dir = os.path.abspath(
        os.path.join(tests_dir, "..", "..", "..", "..", "astro_image_io"))
    if os.path.exists(os.path.join(aio_dir, "astro_image_io.dll")):
        env["Path"] = aio_dir + ";" + env["Path"]

    # ---- 0b. 解析通量核对 (numpy 向量化, 独立于 C++) ----
    print("--- 0. 合成数据解析通量核对 (numpy) ---")
    for size in (96, 128):
        sigma_in = synth_flux(size)
        check("numpy 解析 Σin size=%d" % size, True, "%.6g" % sigma_in,
              scope="0b", tag="synth_flux_%d" % size, sigma_in=sigma_in)

    # ---- A/B/C: 运行 C++ 验收 exe, numpy 聚合断言 ----
    exe = find_exe(args.exe, tests_dir)
    if exe is None:
        check("C++ 验收 exe 可定位", False,
              "请用 --exe 指定 drizzle_acceptance_test (或先构建该 target)",
              scope="ENV", tag="exe_locate")
        return
    print("  exe: %s" % exe)
    ok, tail = run_cxx_exe(exe, out_dir, env)
    if not args.skip_run:
        if not ok:
            check("C++ 验收 exe 运行成功", False, tail[:800],
                  scope="ENV", tag="exe_run")
            print("中止: C++ 验收 exe 失败 (请先修复或 --skip-run 只分析)")
            return
        check("C++ 验收 exe 运行成功", True, scope="ENV", tag="exe_run")

    rows = load_jsonl(os.path.join(out_dir, "acceptance_matrix.jsonl"))
    pos_tags = ["north_pole", "south_pole", "equator_ra0", "ra_cross0", "nominal"]
    os_tags = ["os%d_pf%.2f" % (r, pf)
               for r in (1, 2, 3, 4) for pf in (0.10, 0.50, 0.80, 1.00)]
    scale_tags = ["scale%.1f_n%d" % (s, n)
                  for s, n in ((0.5, 2097152), (1.0, 1048576),
                               (2.0, 262144), (3.0, 131072))]
    wide_tags = ["wide_t4", "wide_sip5", "wide_polar", "wide_ra0"]
    print("--- A. 天极/赤道/RA 跨 0/常规位置 ---")
    for tag in pos_tags:
        m = [r for r in rows if r.get("tag") == tag]
        if not m:
            check("[%s] 场景存在" % tag, False, "JSONL 中无此 tag",
                  scope="A", tag=tag)
            continue
        r = m[0]
        check("[%s] FP64 通量闭合" % tag,
              r["rel_closure_fp64"] < GATES["closure_fp64"],
              "%.3e" % r["rel_closure_fp64"],
              scope="A", tag=tag, closure=r["rel_closure_fp64"])
        check("[%s] FP32 vs FP64 + missing" % tag,
              r["max_rel_fp32_vs_fp64"] < GATES["maxrel_fp32"]
              and r["missing"] == 0,
              "%.3e missing=%d" % (r["max_rel_fp32_vs_fp64"], r["missing"]),
              scope="A", tag=tag, maxrel=r["max_rel_fp32_vs_fp64"],
              missing=r["missing"])

    print("--- B. pixfrac x 过采样率 {1,2,3,4} ---")
    for tag in os_tags:
        m = [r for r in rows if r.get("tag") == tag]
        if not m:
            check("[%s] 场景存在" % tag, False, scope="B", tag=tag)
            continue
        r = m[0]
        check("[%s] FP64 闭合" % tag,
              r["rel_closure_fp64"] < GATES["closure_fp64"],
              "%.3e" % r["rel_closure_fp64"],
              scope="B", tag=tag, closure=r["rel_closure_fp64"])
        check("[%s] FP32 vs FP64" % tag,
              r["max_rel_fp32_vs_fp64"] < GATES["maxrel_fp32"]
              and r["missing"] == 0,
              "%.3e missing=%d" % (r["max_rel_fp32_vs_fp64"], r["missing"]),
              scope="B", tag=tag, maxrel=r["max_rel_fp32_vs_fp64"],
              missing=r["missing"])

    print("--- C. 球面<->平面双向投影 (TAN 往返) ---")
    bidir = [r for r in rows if r.get("tag") == "bidirectional_tan"]
    if bidir:
        r = bidir[0]
        check("TAN 往返像素误差", r["tan_max_px_err"] < GATES["tan_px_err"],
              "%.3e px, 天球 %.3e\"" % (r["tan_max_px_err"],
                                        r["tan_max_sky_err_arcsec"]),
              scope="C", tag="bidirectional_tan", px_err=r["tan_max_px_err"],
              sky_err=r["tan_max_sky_err_arcsec"])
    else:
        check("TAN 往返场景存在", False, scope="C", tag="bidirectional_tan")

    # ---- D. 尺度 x NSIDE 矩阵 ----
    print("--- D. 尺度 x NSIDE 矩阵 (0.5~3\") ---")
    for tag in scale_tags:
        m = [r for r in rows if r.get("tag") == tag]
        if not m:
            check("[%s] 场景存在" % tag, False, scope="D", tag=tag)
            continue
        r = m[0]
        # 阈值 1e-6 是本段原样使用的字面量 (与 GATES["closure_fp64"]=1e-7 不同);
        # 本单只修记账漏计, 不动任何阈值。
        check("[%s] FP64 闭合" % tag,
              r["rel_closure_fp64"] < 1e-6, "%.3e" % r["rel_closure_fp64"],
              scope="D", tag=tag, closure=r["rel_closure_fp64"])
        check("[%s] FP32 vs FP64" % tag,
              r["max_rel_fp32_vs_fp64"] < GATES["maxrel_fp32"]
              and r["missing"] == 0,
              "%.3e missing=%d" % (r["max_rel_fp32_vs_fp64"], r["missing"]),
              scope="D", tag=tag, maxrel=r["max_rel_fp32_vs_fp64"],
              missing=r["missing"])

    # ---- E. 广域大畸变矩阵 ----
    print("--- E. 广域大畸变矩阵 ---")
    for tag in wide_tags:
        m = [r for r in rows if r.get("tag") == tag]
        if not m:
            check("[%s] 场景存在" % tag, False, scope="E", tag=tag)
            continue
        r = m[0]
        check("[%s] FP64 闭合" % tag,
              r["rel_closure_fp64"] < GATES["closure_fp64"],
              "%.3e" % r["rel_closure_fp64"],
              scope="E", tag=tag, closure=r["rel_closure_fp64"])
        check("[%s] FP32 vs FP64" % tag,
              r["max_rel_fp32_vs_fp64"] < GATES["maxrel_fp32"]
              and r["missing"] == 0,
              "%.3e missing=%d" % (r["max_rel_fp32_vs_fp64"], r["missing"]),
              scope="E", tag=tag, maxrel=r["max_rel_fp32_vs_fp64"],
              missing=r["missing"])

    # ---- F. 数值类型审计 (生产源码) ----
    print("--- F. 数值类型审计 (仅 IEEE float32/float64) ---")
    module_dir = os.path.abspath(os.path.join(tests_dir, ".."))
    bad = audit_source_types(module_dir)
    check("生产源码无 long double/half/__float128", len(bad) == 0,
          "" if not bad else "; ".join("%s:%d %s" % b for b in bad[:3]),
          scope="F", tag="source_types", hits=len(bad))

    # ---- G. 标准 ULP + 候选零漏选 ----
    print("--- G. 标准 ULP 分布 + 候选零漏选 ---")
    ulp_path = os.path.join(out_dir, "ulp_distribution.json")
    if os.path.exists(ulp_path):
        with open(ulp_path, "r", encoding="utf-8") as f:
            ulp = json.load(f)
        check("ULP p95", ulp.get("p95", 1e9) < GATES["ulp_p95"],
              "p95=%.1f (n=%d)" % (ulp.get("p95", -1), ulp.get("count", 0)),
              scope="G", tag="ulp_p95", p95=ulp.get("p95"),
              count=ulp.get("count"))
        check("ULP max", ulp.get("max", 1e9) < GATES["ulp_max"],
              "max=%.1f" % ulp.get("max", -1),
              scope="G", tag="ulp_max", max=ulp.get("max"))
    else:
        check("ulp_distribution.json 存在", False, "未找到 %s" % ulp_path,
              scope="G", tag="ulp_file")

    cand_path = os.path.join(out_dir, "candidate_matrix.jsonl")
    cands = load_jsonl(cand_path)
    if cands:
        fn_total = int(np.sum([c["false_negatives"] for c in cands]))
        check("候选零漏选 (全矩阵)", fn_total == 0,
              "%d cases, FN=%d" % (len(cands), fn_total),
              scope="G", tag="candidate", cases=len(cands), fn=fn_total)
    else:
        check("candidate_matrix.jsonl 存在", False,
              scope="G", tag="candidate_file")

    # ---- H. 负例控制 (非退化证据): 旧 pixfrac^2 口径必须判红 ----
    # 同一可执行、同一判据, 只把 Sigma_out 乘回 pixfrac^2 (即「按 A_pixel 归一」
    # 的旧口径 = provenance.flux_conservation_factor 写成 pixfrac^2)。
    # 该模式必须 exit != 0; 否则说明正例门是恒真门, 没有证据资格。
    print("--- H. 负例控制: 注入旧 pixfrac^2 口径 (必须判红) ---")
    if not args.skip_run:
        neg_dir = os.path.join(out_dir, "negative_injection")
        os.makedirs(neg_dir, exist_ok=True)
        neg_ok, neg_tail = run_cxx_exe(exe, neg_dir, env,
                                       ("--inject-legacy-pixfrac2",))
        bad = [l for l in neg_tail.splitlines() if "[FAIL]" in l]
        check("注入 pixfrac^2 后判据判红 (exit != 0)",
              (not neg_ok) and len(bad) > 0,
              "exit=%s, FAIL 行=%d" % ("0" if neg_ok else "非0", len(bad)),
              scope="H", tag="inject_legacy_pixfrac2",
              exe_exit_zero=bool(neg_ok), fail_lines=len(bad))
    else:
        print("  (--skip-run: 跳过负例注入)")
        # 跳过既不计通过也不计失败, 但必须留痕, 否则摘要里会凭空少一项。
        results["skipped"].append({
            "scope": "H", "tag": "inject_legacy_pixfrac2",
            "reason": "--skip-run: 未运行负例注入, 不计通过也不计失败"})


if __name__ == "__main__":
    sys.exit(main())
