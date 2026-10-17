#!/usr/bin/env python3
"""编译并运行 Issue #12 严格等距 Simpson 域单元测试。

直接以 g++ 编译 `eng/tests/unit/simpson_integrator_domain_test.cpp` 与**当前树内**
`lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp`（不链接旧归档，确保测的是
本轮修复后的源码）；不新增 CMake 目标。判据在测试程序内并打印实测读数。

用法:
    python3 eng/tests/unit/run_simpson_integrator_domain_test.py
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SRC_TEST = os.path.join(REPO, "eng", "tests", "unit", "simpson_integrator_domain_test.cpp")
SRC_IMPL = os.path.join(REPO, "lib", "algorithms", "photometry", "cpp", "src",
                        "spectrum_integrator.cpp")
INCLUDES = ["lib/algorithms/photometry/cpp/src", "lib/algorithms/photometry/cpp/include"]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/tmp/simpson_integrator_domain_test")
    ap.add_argument("--impl", default=SRC_IMPL, help="被测实现源文件（A/B 时可指向副本）")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()
    cmd = ["g++", "-std=c++17", "-O1"]
    for i in INCLUDES:
        cmd += ["-I", os.path.join(REPO, i)]
    cmd += [SRC_TEST, a.impl, "-o", a.out]
    cp = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    if cp.returncode != 0:
        print(cp.stderr[-4000:])
        print("COMPILE FAILED")
        return 1
    run = subprocess.run([a.out], cwd=REPO, capture_output=True, text=True)
    if not a.quiet:
        print(run.stdout, end="")
    else:
        tail = [l for l in run.stdout.splitlines() if l.startswith("== checks")]
        print("\n".join(tail))
    if run.stderr:
        print(run.stderr[-2000:], file=sys.stderr)
    return run.returncode


if __name__ == "__main__":
    sys.exit(main())
