#!/usr/bin/env python3
"""编译并运行 Issue #106 参数化窄带响应的 C++ 单元测试。

直接以 g++ 编译 `eng/tests/unit/narrowband_response_test.cpp` 与**当前树内**
`lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp`（不链接旧归档, 确保
测的是本轮实现后的源码）；不新增 CMake 目标。判据在测试程序内并打印实测读数。

用法:
    python3 eng/tests/unit/run_narrowband_response_test.py
    python3 eng/tests/unit/run_narrowband_response_test.py --cases <file>   # 驱动模式
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SRC_TEST = os.path.join(REPO, "eng", "tests", "unit", "narrowband_response_test.cpp")
SRC_IMPL = os.path.join(REPO, "lib", "algorithms", "photometry", "cpp", "src",
                        "spectrum_integrator.cpp")
INCLUDES = ["lib/algorithms/photometry/cpp/src", "lib/algorithms/photometry/cpp/include"]


def build(out: str, impl: str) -> int:
    cmd = ["g++", "-std=c++17", "-O1"]
    for i in INCLUDES:
        cmd += ["-I", os.path.join(REPO, i)]
    cmd += [SRC_TEST, impl, "-o", out]
    cp = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    if cp.returncode != 0:
        print(cp.stderr[-4000:])
        print("COMPILE FAILED")
        return 1
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/tmp/narrowband_response_test")
    ap.add_argument("--impl", default=SRC_IMPL, help="被测实现源文件（A/B 时可指向副本）")
    ap.add_argument("--cases", default=None, help="驱动模式: 用例文件")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()
    if build(a.out, a.impl) != 0:
        return 1
    argv = [a.out]
    if a.cases:
        argv += ["--cases", a.cases]
    run = subprocess.run(argv, cwd=REPO, capture_output=True, text=True)
    if not a.quiet:
        print(run.stdout, end="")
    if run.stderr:
        print(run.stderr[-2000:], file=sys.stderr)
    return run.returncode


if __name__ == "__main__":
    sys.exit(main())
