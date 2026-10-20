#!/usr/bin/env python3
"""编译并运行 Issue #106 的**生产路径**验证（真实 Gaia DR3 XP 光谱）。

与 `run_narrowband_response_test.py` 的区别: 本脚本
  1. 在临时目录里搭一个 `config/response_curves/`（真实曲线库 + **测试专用**的
     等价矩形曲线, 按库的 provenance/curve_stats 格式生成 ⇒ 身份核对通过）;
  2. 把测试程序编译到该临时目录并从那里运行（曲线库路径 = <可执行文件目录>/
     config/response_curves, 见 filter_curve_json.h）;
  3. 用**真实** `gaia/GaiaDR3SP` 光谱库跑 fit_frame_photometry。

链接的是**当前树已构建**的静态库（build/linux-control/libacsd_phase1_photcal.a
等）, 因此测的是本轮实现后的生产代码。

用法:
    python3 eng/tests/unit/run_narrowband_production_path_test.py
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SRC_TEST = os.path.join(REPO, "eng", "tests", "unit", "narrowband_production_path_test.cpp")
BUILD = os.path.join(REPO, "build", "linux-control")
CURVE_DIR = os.path.join(REPO, "config", "response_curves")
GAIA_DIR = os.path.join(REPO, "gaia", "GaiaDR3SP")

INCLUDES = [
    "lib/algorithms/photometry/cpp/src",
    "lib/algorithms/photometry/cpp/include",
    "lib/infrastructure/gaia_xpsd_client/src",
    "lib/infrastructure/aio/src",
    "lib/algorithms/shared",
    "lib/algorithms/shared/include",
    "lib/include",
    "lib/third_party",
]
LIBS = [
    "acsd_phase1_photcal", "acsd_p1_ipv", "acsd_calibration",
    "acsd_identifiability", "acsd_common", "z",
]

RECT_NAME = "ACSD-TEST-Ha-3nm-rectangle"


def make_rectangle_curve(path: str, center_nm: float, fwhm_nm: float) -> None:
    """生成**等价矩形**曲线（测试夹具; 不进生产曲线库）。

    曲线文件必须满足库的身份核对: name / channel / n_points / provenance
    .curve_stats（含逐元素指纹 wl_sum/val_sum/val_sumsq）。
    """
    lo = center_nm - fwhm_nm / 2.0
    hi = center_nm + fwhm_nm / 2.0
    step = 0.25
    n = int(round((672.0 - 640.0) / step)) + 1
    wl = [640.0 + i * step for i in range(n)]
    val = [1.0 if (lo <= w <= hi) else 0.0 for w in wl]
    stats = {
        "n_points": n,
        "wl_min": min(wl), "wl_max": max(wl),
        "val_min": min(val), "val_max": max(val),
        "wl_sum": sum(wl), "val_sum": sum(val),
        "val_sumsq": sum(v * v for v in val),
    }
    doc = {
        "name": RECT_NAME,
        "channel": "HA",
        "n_points": n,
        "wavelength_nm": wl,
        "value": val,
        "provenance": {
            "source": "ACSD Issue #106 test fixture (equivalent rectangle)",
            "note": "parametric_narrowband top_hat_approx/v1 rendered as a sampled "
                    "curve, for the same-input measured-vs-parametric comparison",
            "center_nm": center_nm, "fwhm_nm": fwhm_nm,
            "curve_stats": stats,
        },
    }
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(doc, fh, indent=1)


def build(out_dir: str) -> str:
    exe = os.path.join(out_dir, "narrowband_production_path_test")
    cmd = ["g++", "-std=c++17", "-O2", "-fopenmp"]
    for i in INCLUDES:
        cmd += ["-I", os.path.join(REPO, i)]
    cmd += [SRC_TEST, "-o", exe]
    cmd += [f"-L{BUILD}"] + [f"-l{n}" for n in LIBS] + ["-lpthread"]
    cp = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    if cp.returncode != 0:
        print(cp.stderr[-6000:])
        print("COMPILE FAILED")
        return ""
    return exe


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--work", default="/tmp/acsd_narrowband_prod")
    ap.add_argument("--out", default="/tmp/narrowband_production_path.json")
    ap.add_argument("--center-nm", type=float, default=656.28)
    ap.add_argument("--fwhm-nm", type=float, default=3.0)
    ap.add_argument("--ra", type=float, default=100.0)
    ap.add_argument("--dec", type=float, default=0.0)
    a = ap.parse_args()

    if not os.path.isdir(Gaia_DIR := os.path.join(REPO, "gaia", "GaiaDR3SP")):
        print(f"Gaia DR3SP data not found: {Gaia_DIR}")
        return 2
    if not os.path.isdir(BUILD):
        print(f"build dir not found: {BUILD} (build the acsd target first)")
        return 2

    work = os.path.abspath(a.work)
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(os.path.join(work, "config", "response_curves"), exist_ok=True)
    for name in os.listdir(CURVE_DIR):
        if name.endswith(".json"):
            shutil.copy2(os.path.join(CURVE_DIR, name),
                         os.path.join(work, "config", "response_curves", name))
    make_rectangle_curve(
        os.path.join(work, "config", "response_curves", RECT_NAME + ".json"),
        a.center_nm, a.fwhm_nm)

    exe = build(work)
    if not exe:
        return 1

    cmd = [exe, "--gaia", Gaia_DIR, "--out", a.out,
           "--rect", RECT_NAME,
           "--center-nm", repr(a.center_nm), "--fwhm-nm", repr(a.fwhm_nm),
           "--ra", repr(a.ra), "--dec", repr(a.dec)]
    run = subprocess.run(cmd, cwd=work, capture_output=True, text=True)
    sys.stdout.write(run.stdout)
    if run.stderr:
        sys.stderr.write(run.stderr[-6000:])
    print(f"exit code: {run.returncode}")
    return run.returncode


if __name__ == "__main__":
    sys.exit(main())
