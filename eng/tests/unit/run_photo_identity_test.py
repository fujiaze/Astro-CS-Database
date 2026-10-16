#!/usr/bin/env python3
"""编译并运行施加标度身份单元测试（eng/tests/unit/photo_identity_test.cpp）。

与仓库既有 C++ 单测同款做法（见 eng/tests/conformance/normalize_frame_science_schema_test.py）:
直接调用 g++ 编译单测 + 链接已构建的静态库, 不新增 CMake 目标。

判据所有权: 本脚本只负责编译/运行与如实回报退出码; PASS/FAIL 判定在测试程序内
（每条都打印实测读数）。

用法:
    python3 eng/tests/unit/run_photo_identity_test.py [--build-dir build/linux-sci-p5]
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SRC = os.path.join(REPO, "eng", "tests", "unit", "photo_identity_test.cpp")

INCLUDES = [
    "lib/include",
    "lib/algorithms/noise_snr/include",
    "lib/algorithms/integration/normalize_science/include",
    "lib/algorithms/drizzle/healpix_drizzle",
    "lib/third_party",
    "lib/infrastructure/aio/src",
    "lib/infrastructure/aio/include",
    "lib/algorithms/shared",
    "lib/algorithms/psf/include",
    "lib/infrastructure/scheduler/src",
]

# 归档提供 normalize_precision.cpp.o（module_adapters）及其依赖; 需要 --start-group
# 解循环依赖。crypto/healpix 以源文件直接参与（单测直接调用其符号）。
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
    "libacsd_calibration.a",
    "libacsd_io_adapter.a",
    "libacsd_p1_sdet.a",
    "libacsd_p1_dpsf.a",
    "libacsd_identifiability.a",
]
SRCS = ["lib/algorithms/shared/crypto/sha256.cpp",
        "lib/algorithms/shared/healpix/healpix_core.cpp"]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default="build/linux-sci-p5")
    ap.add_argument("--out", default="/tmp/photo_identity_test")
    a = ap.parse_args()
    bd = os.path.join(REPO, a.build_dir)
    if not os.path.isdir(bd):
        print("SKIP: build dir not found: %s" % bd)
        return 0
    missing = [x for x in ARCHIVES if not os.path.exists(os.path.join(bd, x))]
    if missing:
        print("SKIP: missing archives in %s: %s" % (bd, ", ".join(missing[:3])))
        return 0
    cmd = ["g++", "-std=c++17", "-O1"]
    for i in INCLUDES:
        cmd += ["-I", os.path.join(REPO, i)]
    cmd += [SRC]
    cmd += ["-Wl,--start-group"]
    cmd += [os.path.join(bd, x) for x in ARCHIVES]
    cmd += [os.path.join(REPO, s) for s in SRCS]
    cmd += ["-Wl,--end-group", "-lz", "-lpthread", "-o", a.out]
    print("compile: %s" % " ".join(cmd[:6]) + " ...")
    cp = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    if cp.returncode != 0:
        print(cp.stderr[-4000:])
        print("COMPILE FAILED")
        return 1
    run = subprocess.run([a.out], cwd=REPO, capture_output=True, text=True)
    print(run.stdout, end="")
    if run.stderr:
        print(run.stderr[-2000:], file=sys.stderr)
    return run.returncode


if __name__ == "__main__":
    sys.exit(main())
