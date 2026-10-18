#!/usr/bin/env python3
"""编译并运行 Issue #9 冻结单位表（线性基单位）单元测试。

以**当前树内**的唯一实现编译（不链接旧归档）:
  lib/algorithms/resample/p3_rsmp_units.cpp（冻结单位表）

用法:
    python3 eng/tests/unit/run_p3_rsmp_units_test.py
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
TEST = os.path.join(REPO, "eng", "tests", "unit", "p3_rsmp_units_test.cpp")
IMPL = os.path.join(REPO, "lib", "algorithms", "resample")
SRCS = [
    os.path.join(IMPL, "p3_rsmp_units.cpp"),
    TEST,
]


def main() -> int:
    for s in SRCS:
        if not os.path.isfile(s):
            print(f"missing source: {s}", file=sys.stderr)
            return 2
    with tempfile.TemporaryDirectory(prefix="issue9_units_") as td:
        exe = os.path.join(td, "p3_rsmp_units_test")
        cmd = ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra",
               "-I", IMPL, "-o", exe] + SRCS
        print("+ " + " ".join(cmd))
        rc = subprocess.run(cmd).returncode
        if rc != 0:
            print(f"compile failed rc={rc}", file=sys.stderr)
            return rc
        return subprocess.run([exe]).returncode


if __name__ == "__main__":
    sys.exit(main())
