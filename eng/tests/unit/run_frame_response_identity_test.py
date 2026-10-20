#!/usr/bin/env python3
"""编译并运行 Issue #106 独立审核 P1 的**逐帧响应身份**测试。

测试程序: eng/tests/unit/frame_response_identity_test.cpp
  · 真实 Gaia DR3 XP 光谱（仓内 gaia/GaiaDR3SP）+ **真实**响应曲线库
    (config/response_curves) ⇒ 逐帧响应 id 来自**生产拟合入口**
    `fit_frame_photometry`, 不是手写假值;
  · 逐帧落盘/解析用**生产同一份**实现
    (lib/infrastructure/scheduler/src/photometry_frame_identity.h);
  · frame_science 身份用**生产**函数 (photo_response_identity /
    photo_application_identity)。

曲线库路径 = <可执行文件目录>/config/response_curves（filter_curve_json.h 的
唯一机制）⇒ 本脚本把曲线库复制到临时工作目录并从那里运行测试程序。

用法:
    python3 eng/tests/unit/run_frame_response_identity_test.py [--build-dir build/linux-control]
退出码 = 测试程序退出码（0 = 全过）。
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SRC_TEST = os.path.join(REPO, "eng", "tests", "unit", "frame_response_identity_test.cpp")
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
    # 调度层头（逐帧身份实现 + 规范身份 API）
    "lib/infrastructure/scheduler/src",
    "lib/algorithms/integration/normalize_science/include",
    "lib/algorithms/noise_snr/include",
    "lib/algorithms/psf/include",
    "lib/algorithms/drizzle/healpix_drizzle",
    "lib/infrastructure/aio/include",
]

# 静态库（与 run_photo_identity_test.py 同款链接面）: 施加身份函数来自
# libacsd_module_adapters.a（normalize_precision.cpp.o 在其中）。
ARCHIVES = [
    "libacsd_module_adapters.a",
    "lib/algorithms/integration/normalize_science/libacsd_normalize_science.a",
    "libacsd_phase1_noise.a",
    "libacsd_drizzle.a",
    "libacsd_aio.a",
    "libacsd_cfitsio.a",
    "libacsd_hips.a",
    "libacsd_core.a",
    "libacsd_phase1_wcs.a",
    "libacsd_phase1_stars.a",
    "libacsd_phase1_photcal.a",
    "libacsd_p1_ipv.a",
    "libacsd_calibration.a",
    "libacsd_io_adapter.a",
    "libacsd_p1_sdet.a",
    "libacsd_p1_dpsf.a",
    "libacsd_identifiability.a",
]
SRCS = [
    "lib/algorithms/shared/crypto/sha256.cpp",
    "lib/algorithms/shared/healpix/healpix_core.cpp",
]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default="build/linux-control")
    ap.add_argument("--work", default="/tmp/acsd_frame_response_identity")
    ap.add_argument("--ra", type=float, default=100.0)
    ap.add_argument("--dec", type=float, default=0.0)
    a = ap.parse_args()

    bd = os.path.join(REPO, a.build_dir)
    if not os.path.isdir(bd):
        print(f"SKIP: build dir not found: {bd}")
        return 0
    missing = [x for x in ARCHIVES if not os.path.exists(os.path.join(bd, x))]
    if missing:
        print(f"SKIP: missing archives in {bd}: {missing[:3]}")
        return 0
    if not os.path.isdir(GAIA_DIR):
        print(f"SKIP: Gaia DR3 SP data not found: {GAIA_DIR}")
        return 0

    work = os.path.abspath(a.work)
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(os.path.join(work, "config", "response_curves"), exist_ok=True)
    for name in os.listdir(CURVE_DIR):
        if name.endswith(".json"):
            shutil.copy2(os.path.join(CURVE_DIR, name),
                         os.path.join(work, "config", "response_curves", name))

    exe = os.path.join(work, "frame_response_identity_test")
    cmd = ["g++", "-std=c++17", "-O1", "-fopenmp"]
    for i in INCLUDES:
        cmd += ["-I", os.path.join(REPO, i)]
    cmd += [SRC_TEST]
    cmd += ["-Wl,--start-group"]
    cmd += [os.path.join(bd, x) for x in ARCHIVES]
    cmd += [os.path.join(REPO, s) for s in SRCS]
    cmd += ["-Wl,--end-group", "-lz", "-lpthread", "-o", exe]
    cp = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    if cp.returncode != 0:
        print(cp.stderr[-8000:])
        print("COMPILE FAILED")
        return 1

    run = subprocess.run(
        [exe, "--gaia", GAIA_DIR, "--ra", repr(a.ra), "--dec", repr(a.dec)],
        cwd=work, capture_output=True, text=True)
    sys.stdout.write(run.stdout)
    if run.stderr:
        sys.stderr.write(run.stderr[-8000:])
    print(f"exit code: {run.returncode}")
    return run.returncode


if __name__ == "__main__":
    sys.exit(main())
