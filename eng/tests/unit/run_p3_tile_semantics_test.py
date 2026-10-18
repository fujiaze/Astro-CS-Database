#!/usr/bin/env python3
"""Issue #87 tile 完整性分类/缓存/聚合单元测试 —— 编译并运行真实生产库。

被测源 = 当前树的 `aio_hips_reader.cpp` + `p3_resample.cpp`（经构建产物链接，
不链接旧归档）。受控夹具由 `实验/issue87-stage3-tile-integrity/make_fixtures.py`
生成；本脚本用 healpy 计算 tile 中心后传入 C++ 测试。

用法:
  python3 eng/tests/unit/run_p3_tile_semantics_test.py [--build build/linux-dev]
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys

import healpy as hp
import numpy as np

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
TEST = os.path.join(REPO, "eng", "tests", "unit", "p3_tile_semantics_test.cpp")
EXP = os.path.join(REPO, "实验", "issue87-stage3-tile-integrity")
FIXTURES = os.path.join(EXP, "work", "fixtures")

LIBS = [
    "lib/algorithms/resample/libacsd_p3_rsmp.a",
    "libacsd_hips_properties.a",
    "libacsd_hips.a",
    "libacsd_aio.a",
    "libacsd_common.a",
    "libacsd_cfitsio.a",
]
INCLUDES = ["lib/algorithms/resample", "lib/include", "lib/include/acsd",
            "lib/algorithms/shared", "lib/algorithms/shared/crypto",
            "lib/algorithms/shared/healpix", "lib/infrastructure/aio/include"]


def center(order: int, ipix: int) -> tuple[float, float]:
    theta, phi = hp.pix2ang(2 ** order, int(ipix), nest=True)
    return float(np.degrees(phi)), float(90.0 - np.degrees(theta))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default=os.path.join(REPO, "build", "linux-dev"))
    args = ap.parse_args()
    build = os.path.abspath(args.build)
    if not os.path.isdir(FIXTURES):
        print("fixtures missing: run 实验/issue87-stage3-tile-integrity/make_fixtures.py",
              file=sys.stderr)
        return 2
    pts = [center(0, 0), center(0, 4), center(0, 5), center(0, 7),
           center(0, 8), center(1, 5)]
    exe = os.path.join(EXP, "evidence", "p3_tile_semantics_test")
    os.makedirs(os.path.dirname(exe), exist_ok=True)
    cmd = ["g++", "-std=c++17", "-O2", "-o", exe, TEST]
    cmd += ["-I" + os.path.join(REPO, i) for i in INCLUDES]
    cmd += ["-I" + build]
    cmd += [os.path.join(build, l) for l in LIBS]
    cmd += ["-lz", "-lpthread", "-lm"]
    print("+ " + " ".join(cmd))
    rc = subprocess.run(cmd).returncode
    if rc != 0:
        print(f"compile failed rc={rc}", file=sys.stderr)
        return rc
    argv = [exe, FIXTURES] + [f"{v:.12f}" for p in pts for v in p]
    print("+ " + " ".join(argv))
    return subprocess.run(argv).returncode


if __name__ == "__main__":
    sys.exit(main())
