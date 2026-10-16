#!/usr/bin/env python3
"""编译并运行 Issue #10 frame_science leaf_order 真实 reader 测试。

被测 = 生产 `acsd::normalize::read_frame_science_document`
（lib/algorithms/integration/normalize_science）。g++ 直编 + 链接已构建静态库，
不新增 CMake 目标（与 eng/tests/unit/run_photo_identity_test.py 同款）。

用法：
    python3 eng/tests/unit/run_frame_science_leaf_order_reader_test.py \
        <product_dir> <K> [legacy_sidecar.json] \
        [--build-dir build/linux-sci-p5]
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SRC = os.path.join(REPO, "eng", "tests", "unit", "frame_science_leaf_order_reader_test.cpp")

INCLUDES = [
    "lib/include",
    "lib/third_party",
    "lib/algorithms/integration/normalize_science/include",
    "lib/infrastructure/aio/include",
    "lib/infrastructure/aio/io/include",
    "lib/infrastructure/aio/src",
    "lib/algorithms/shared",
]

ARCHIVES = [
    "lib/algorithms/integration/normalize_science/libacsd_normalize_science.a",
    "libacsd_phase1_noise.a",
    "libacsd_io_adapter.a",
    "libacsd_aio.a",
    "libacsd_hips.a",
    "libacsd_cfitsio.a",
    "libacsd_core.a",
    "libacsd_common.a",
]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("product_dir")
    ap.add_argument("k", type=int)
    ap.add_argument("legacy", nargs="?", default=None)
    ap.add_argument("--build-dir", default="build/linux-sci-p5")
    ap.add_argument("--out", default="/tmp/frame_science_leaf_order_reader_test")
    a = ap.parse_args()
    bd = os.path.join(REPO, a.build_dir)
    if not os.path.isdir(bd):
        print("SKIP: build dir not found: %s" % bd)
        return 0
    missing = [x for x in ARCHIVES if not os.path.exists(os.path.join(bd, x))]
    if missing:
        print("SKIP: missing archives: %s" % ", ".join(missing))
        return 0
    cmd = ["g++", "-std=c++17", "-O1", "-fopenmp"]
    for i in INCLUDES:
        cmd += ["-I", os.path.join(REPO, i)]
    cmd += [SRC]
    cmd += ["-Wl,--start-group"]
    cmd += [os.path.join(bd, x) for x in ARCHIVES]
    cmd += ["-Wl,--end-group", "-lz", "-lpthread", "-o", a.out]
    print("compile:", " ".join(cmd))
    if subprocess.run(cmd).returncode != 0:
        print("COMPILE FAILED")
        return 1
    run = [a.out, a.product_dir, str(a.k)]
    if a.legacy:
        run.append(a.legacy)
    return subprocess.run(run).returncode


if __name__ == "__main__":
    sys.exit(main())
