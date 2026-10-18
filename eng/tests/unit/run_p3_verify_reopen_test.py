#!/usr/bin/env python3
"""Issue #94 定向单测：P3FitsVerifyStream 的 P3_OUT_OK && reopen_ok=false 形态。

编译并运行 `p3_verify_reopen_test.cpp`（链接当前构建的生产库）。

用法:
  python3 eng/tests/unit/run_p3_verify_reopen_test.py [--build build/linux-dev]
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
TEST = os.path.join(REPO, "eng", "tests", "unit", "p3_verify_reopen_test.cpp")

LIBS = [
    "lib/algorithms/fits_output/libacsd_p3_fits_output.a",
    "lib/algorithms/projection/libacsd_p3_projection_wcs.a",
    # Issue #9: p3_output 的 BUNIT 判据消费冻结单位表（p3rsmp::resolve_bunit/Bunit）。
    "lib/algorithms/resample/libacsd_p3_rsmp.a",
    "libacsd_core.a",
    "libacsd_aio.a",
    "libacsd_common.a",
    "libacsd_cfitsio.a",
]
INCLUDES = [
    "lib/algorithms/fits_output",
    "lib/algorithms/projection",
    "lib/include",
    "lib/include/acsd",
    "lib/algorithms/shared",
    "lib/infrastructure/aio/include",
]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default=os.path.join(REPO, "build", "linux-dev"))
    args = ap.parse_args()
    build = os.path.abspath(args.build)
    exe = os.path.join(tempfile.mkdtemp(prefix="p3_verify_reopen_"), "t")
    cmd = ["g++", "-std=c++17", "-O2", "-o", exe, TEST]
    cmd += ["-I" + os.path.join(REPO, i) for i in INCLUDES]
    cmd += [os.path.join(build, l) for l in LIBS]
    cmd += ["-lz", "-lpthread", "-lm"]
    print("+ " + " ".join(cmd))
    rc = subprocess.run(cmd).returncode
    if rc != 0:
        print(f"compile failed rc={rc}", file=sys.stderr)
        return rc
    tmp = tempfile.mkdtemp(prefix="p3_verify_reopen_run_")
    try:
        return subprocess.run([exe, tmp]).returncode
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
