#!/usr/bin/env python3
"""编译并运行 Issue #91 输入单位身份裁决单元测试。

以**当前树内**的唯一实现编译（不链接旧归档）:
  lib/algorithms/resample/p3_input_unit.cpp（纯裁决逻辑，无 I/O）
  lib/algorithms/resample/p3_rsmp_units.cpp（冻结单位表 resolve_bunit）

用法:
    python3 eng/tests/unit/run_p3_input_unit_guard_test.py
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
TEST = os.path.join(REPO, "eng", "tests", "unit", "p3_input_unit_guard_test.cpp")
IMPL = os.path.join(REPO, "lib", "algorithms", "resample")
SRCS = [
    os.path.join(IMPL, "p3_input_unit.cpp"),
    os.path.join(IMPL, "p3_rsmp_units.cpp"),
    TEST,
]


def main() -> int:
    for s in SRCS:
        if not os.path.isfile(s):
            print(f"missing source: {s}", file=sys.stderr)
            return 2
    with tempfile.TemporaryDirectory(prefix="issue91_unit_") as td:
        exe = os.path.join(td, "p3_input_unit_guard_test")
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
