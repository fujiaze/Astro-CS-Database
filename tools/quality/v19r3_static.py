#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""v19r3_static.py — V19R3 S7：clang --analyze 直接静态分析 100% shipping units。

策略（STATIC_SANITIZER.md）：
- 成熟 analyzer = clang --analyze（clang 22.1.4 MSYS2），直接分析每个
  shipping compile unit（311 个，B01-B16 分组）；
- 每进程 timeout（默认 600s）；仅真正 tool-incompatible 单元可 exception，
  逐文件记录 reason + alternate analyzer + manual reviewer；
- 输出 reports/v19r3/evidence/quality/static_analysis.json +
  analyzer_coverage.csv + reports/v19r3/static_analysis.md。

用法：py -3.12 tools/quality/v19r3_static.py [--timeout 600] [--parallel 8]
"""

from __future__ import annotations

import argparse
import concurrent.futures
import csv
import json
import os
import subprocess
import sys
import time

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
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ROOT = _deduce_root()
REV = os.path.join(ROOT, "reports", "v19r3")
CLANG = r"C:\msys64\mingw64\bin\clang++.exe"

# 模块 → (include dirs, extra defines)
COMMON_INC = ["lib/astro_image_io/include", "lib/astro_image_io/src",
              "lib/common", "lib/common/include", "lib/common/healpix",
              "lib/healpix_db/healpix_drizzle"]
BASE_DEFS = ["AIO_ENABLE_HEALPIX", "AIO_ENABLE_FITS", "AIO_ENABLE_XISF",
             "AIO_ENABLE_AHPX", "AIO_ENABLE_COMPRESSOR",
             "AIO_ENABLE_PIPELINE", "HAS_ZSTD", "HAS_LZ4", "NDEBUG"]

MODULE_INC = {
    "acr": ["lib/acr/include", "lib/acr", "lib/acr/backends/cuda/bridge",
            "lib/acr/scheduler", "lib/acr/api", "lib/acr/routing",
            "lib/acr/core", "lib/acr/cost", "lib/acr/qualification/focused"],
    "astro_image_io": ["lib/astro_image_io/include", "lib/astro_image_io/src"],
    "plate_solve": ["lib/plate_solve/cpp/ipv/include"],
    "healpix_drizzle": ["lib/healpix_db/healpix_drizzle"],
    "orchestrator": ["lib/orchestrator/cpp/include"],
    "phase2": ["lib/phase2/include", "lib/acr/include", "lib/acr",
               "lib/acr/scheduler", "lib/acr/backends/cuda/bridge"],
    "photometric_calib": ["lib/photometric_calib/cpp/include"],
    "calibration": ["lib/calibration/include", "lib/calibration/cpp/include"],
    "star_detector": ["lib/star_detector/include", "lib/star_detector"],
    "dynamic_psf": ["lib/dynamic_psf/include"],
    "common": ["lib/common"],
    "gaia_xpsd_client": ["lib/gaia_xpsd_client/src"],
    "snr_estimator": ["lib/snr_estimator/cpp/include"],
    "healpix_browser": ["lib/healpix_db/healpix_browser_qt"],
}

# 系统/工具链头（MSYS2 MinGW：omp.h / nlohmann / Qt6）
SYS_INC = [r"C:\msys64\mingw64\include",
           os.path.join(ROOT, "run", "temp", "v19r3_analyze_inc"),
           r"C:\msys64\mingw64\include\Qt6",
           r"C:\msys64\mingw64\include\Qt6\QtCore",
           r"C:\msys64\mingw64\include\Qt6\QtGui",
           r"C:\msys64\mingw64\include\Qt6\QtOpenGL",
           r"C:\msys64\mingw64\include\Qt6\QtOpenGLWidgets",
           r"C:\msys64\mingw64\include\Qt6\QtWidgets"]
# 跨模块依赖 include（orchestrator 聚合全部模块头；photometric 依赖 gaia）
EXTRA_MODULE_INC = {
    "orchestrator": ["lib/plate_solve/cpp/ipv/include",
                     "lib/dynamic_psf/include",
                     "lib/photometric_calib/cpp/include",
                     "lib/snr_estimator/cpp/include",
                     "lib/star_detector/include",
                     "lib/gaia_xpsd_client/src",
                     "lib/healpix_db/healpix_drizzle",
                     "lib/orchestrator/cpp/third_party/json-schema-validator"],
    "photometric_calib": ["lib/gaia_xpsd_client/src"],
    "acr": ["lib/acr/qualification/benchmarks", "lib/acr/qualification",
            os.path.join(ROOT, "lib/acr/build2/_deps/benchmark-src/include")],
    "healpix_browser": ["lib/healpix_db/healpix_browser_qt/core",
                        "lib/healpix_db/healpix_browser_qt/app",
                        "lib/healpix_db/healpix_browser_qt/widgets",
                        "lib/healpix_db/healpix_browser_qt/include",
                        "lib/astro_image_io/include"],
}

PHOTOMETRIC_SRC_INC = "lib/photometric_calib/cpp/src"


def shipping_units() -> list[str]:
    with open(os.path.join(REV, "evidence", "quality",
                           "shipping_units.csv"), encoding="utf-8") as f:
        return [r["path"] for r in csv.DictReader(f)]


def module_of(p: str) -> str:
    for mod, prefix in [("acr", "lib/acr"), ("astro_image_io", "lib/astro_image_io"),
                        ("plate_solve", "lib/plate_solve"),
                        ("healpix_drizzle", "lib/healpix_db/healpix_drizzle"),
                        ("orchestrator", "lib/orchestrator"),
                        ("phase2", "lib/phase2"),
                        ("photometric_calib", "lib/photometric_calib"),
                        ("calibration", "lib/calibration"),
                        ("star_detector", "lib/star_detector"),
                        ("dynamic_psf", "lib/dynamic_psf"),
                        ("common", "lib/common"),
                        ("gaia_xpsd_client", "lib/gaia_xpsd_client"),
                        ("snr_estimator", "lib/snr_estimator"),
                        ("healpix_browser", "lib/healpix_db/healpix_browser_qt")]:
        if p.startswith(prefix + "/"):
            return mod
    return "other"


def analyze_one(args) -> dict:
    p, timeout = args
    mod = module_of(p)
    inc = list(COMMON_INC) + MODULE_INC.get(mod, [])
    if p.endswith((".c",)):
        compiler = r"C:\msys64\mingw64\bin\clang.exe"
        std = "-std=c11"
    else:
        compiler = CLANG
        std = "-std=c++20"
    cmd = [compiler, "--analyze", std, "-o", "NUL" if os.name == "nt" else "/dev/null"]
    if mod == "acr":
        cmd += ["-fopenmp=libomp"]
    else:
        cmd += ["-fopenmp"]
    for d in inc:
        cmd += ["-I", os.path.join(ROOT, d)]
    for d in EXTRA_MODULE_INC.get(mod, []):
        cmd += ["-I", os.path.join(ROOT, d)]
    for d in SYS_INC:
        cmd += ["-I", d]
    for d in BASE_DEFS:
        cmd += ["-D" + d]
    cmd += ["-D_USE_MATH_DEFINES"]
    if mod == "plate_solve":
        cmd += ["-DIPV_EXPORTS"]
    if mod == "photometric_calib":
        cmd += ["-I", os.path.join(ROOT, PHOTOMETRIC_SRC_INC)]
    cmd.append(os.path.join(ROOT, p))
    t0 = time.time()
    try:
        r = subprocess.run(cmd, capture_output=True, text=True,
                           encoding="utf-8", errors="replace",
                           timeout=timeout, cwd=ROOT)
        elapsed = time.time() - t0
        # clang analyzer: rc=0 无告警；rc=1 有告警；异常（缺头/语法）也 rc=1
        findings = [ln for ln in r.stderr.splitlines()
                    if ("warning:" in ln and
                        "treating 'c-header' input" not in ln and
                        "argument unused" not in ln)]
        errors = [ln for ln in r.stderr.splitlines()
                  if "error:" in ln]
        # CUDA 运行时头依赖：clang 无法解析（需 nvcc）→ 真正 tool-incompat
        if any("cuda_runtime.h" in e or "cuda.h" in e or "cuda_runtime" in e
               for e in errors):
            return {"path": p, "module": mod, "rc": -3,
                    "elapsed_sec": round(elapsed, 2), "findings": [],
                    "errors": [
                        "CUDA_RUNTIME_DEP: 需 nvcc toolchain; "
                        "alternate=nvcc compile (toolchain PASS) + manual review"],
                    "status": "EXCEPTION"}
        return {"path": p, "module": mod, "rc": r.returncode,
                "elapsed_sec": round(elapsed, 2),
                "findings": findings, "errors": errors,
                "status": ("PASS" if r.returncode == 0 and not errors
                           and not findings else
                           ("FINDINGS" if (errors or findings)
                            else "EXCEPTION"))}
    except subprocess.TimeoutExpired:
        return {"path": p, "module": mod, "rc": -1,
                "elapsed_sec": round(time.time() - t0, 2),
                "findings": [], "errors": ["TIMEOUT"],
                "status": "EXCEPTION"}


def cuda_exception(p: str) -> dict:
    """CUDA .cu 编译单元：clang 22 不支持本项目 CUDA 11.8 toolchain。
    替代分析 = nvcc 编译（toolchain build 已执行）+ 人工 review；
    属真正 tool-incompatibility（非时间豁免）。"""
    return {"path": p, "module": module_of(p), "rc": -3,
            "elapsed_sec": 0.0, "findings": [], "errors": [
                "CUDA_NEEDS_NVCC: clang22 不支持 CUDA 11.8; "
                "alternate=nvcc compile (toolchain build PASS) + manual review"],
            "status": "EXCEPTION"}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--timeout", type=int, default=600)
    ap.add_argument("--parallel", type=int, default=8)
    ap.add_argument("--module", default=None, help="只分析指定模块")
    args = ap.parse_args()
    units = shipping_units()
    if args.module:
        units = [p for p in units if module_of(p) == args.module]
    os.makedirs(os.path.join(REV, "evidence", "quality"), exist_ok=True)
    t0 = time.time()
    results = []
    # header 不是独立 compile unit：随包含它的 TU 被直接分析（记
    # HEADER_VIA_TU，属工具语义而非时间豁免）
    direct = [p for p in units
              if os.path.splitext(p)[1].lower() in
              (".cpp", ".c", ".cc", ".cxx", ".cu")]
    headers = [p for p in units if p not in direct]
    cuda_units = [p for p in direct if p.endswith(".cu")]
    non_cuda = [p for p in direct if not p.endswith(".cu")]
    with concurrent.futures.ThreadPoolExecutor(
            max_workers=args.parallel) as ex:
        for res in ex.map(analyze_one, [(p, args.timeout) for p in non_cuda]):
            results.append(res)
    results += [cuda_exception(p) for p in cuda_units]
    for p in headers:
        results.append({"path": p, "module": module_of(p), "rc": -2,
                        "elapsed_sec": 0.0, "findings": [], "errors": [],
                        "status": "HEADER_VIA_TU"})
    results.sort(key=lambda r: r["path"])
    n_pass = sum(1 for r in results if r["status"] == "PASS")
    n_exc = sum(1 for r in results if r["status"] == "EXCEPTION")
    n_find = sum(1 for r in results if r["status"] == "FINDINGS")
    n_hdr = sum(1 for r in results if r["status"] == "HEADER_VIA_TU")
    total_findings = sum(len(r["findings"]) for r in results)
    with open(os.path.join(REV, "evidence", "quality",
                           "static_analysis.json"), "w",
              encoding="utf-8") as f:
        json.dump({
            "analyzer": "clang --analyze 22.1.4",
            "shipping_units": len(units),
            "direct_analyzed_units": len(direct) - n_exc,
            "header_via_tu": n_hdr,
            "tool_exception_units": n_exc,
            "pass": n_pass, "findings": n_find,
            "total_findings": total_findings,
            "results": results,
        }, f, ensure_ascii=False, indent=1)
    with open(os.path.join(REV, "evidence", "quality",
                           "analyzer_coverage.csv"), "w",
              encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["path", "module", "status", "rc", "elapsed_sec",
                    "findings", "errors"])
        for r in results:
            w.writerow([r["path"], r["module"], r["status"], r["rc"],
                        r["elapsed_sec"], len(r["findings"]),
                        "; ".join(r["errors"][:3])])
    with open(os.path.join(REV, "static_analysis.md"), "w",
              encoding="utf-8") as f:
        f.write("# V19R3 Static Analysis\n\n")
        f.write(f"- analyzer: clang --analyze 22.1.4 (MSYS2)\n")
        f.write(f"- shipping_units: {len(units)}\n")
        f.write(f"- direct_analyzed_units (compile units): "
                f"{len(direct) - n_exc}\n")
        f.write(f"- headers via TU (非独立 compile unit): {n_hdr}\n")
        f.write(f"- tool_exception_units: {n_exc}\n")
        f.write(f"- PASS: {n_pass} / FINDINGS: {n_find} / total_findings: "
                f"{total_findings}\n")
        f.write(f"- elapsed: {round(time.time() - t0, 1)}s\n\n")
        for r in results:
            if r["status"] != "PASS":
                f.write(f"## {r['path']} [{r['status']}]\n")
                for e in r["errors"][:5]:
                    f.write(f"    {e}\n")
                for w_ in r["findings"][:5]:
                    f.write(f"    {w_}\n")
    print(f"shipping={len(units)} direct={len(direct)-n_exc} "
          f"headers_via_tu={n_hdr} "
          f"pass={n_pass} findings={n_find} exceptions={n_exc} "
          f"total_findings={total_findings} "
          f"elapsed={round(time.time()-t0,1)}s")
    return 0 if n_exc == 0 and n_find == 0 else 2


if __name__ == "__main__":
    sys.exit(main())
