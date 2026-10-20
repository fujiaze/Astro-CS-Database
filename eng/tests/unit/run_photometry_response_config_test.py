#!/usr/bin/env python3
"""编译并运行 Issue #106 响应来源 one-of 的站点解析单元测试。

直接以 g++ 编译 `eng/tests/unit/photometry_response_config_test.cpp` + 仓内
`lib/infrastructure/scheduler/src/photometry_response_config.h`（header-only,
**不链接** module_adapters ⇒ 测的是该解析器本身）; 不新增 CMake 目标。

用法:
    python3 eng/tests/unit/run_photometry_response_config_test.py
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SRC_TEST = os.path.join(REPO, "eng", "tests", "unit", "photometry_response_config_test.cpp")
INCLUDES = [
    "lib/infrastructure/scheduler/src",
    "lib/include",
    "lib/third_party",
]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/tmp/photometry_response_config_test")
    a = ap.parse_args()
    cmd = ["g++", "-std=c++17", "-O1"]
    for i in INCLUDES:
        cmd += ["-I", os.path.join(REPO, i)]
    cmd += [SRC_TEST, "-o", a.out]
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
