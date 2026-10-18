#!/usr/bin/env python3
"""Issue #87/E2-R：FITS tile checksum 快速路径 vs 独立 CFITSIO 对拍驱动。

编译并运行 `checksum_oracle.cpp`（链接当前树的真实生产库），把每个用例的
reader 返回码、cfitsio `fits_verify_chksum` oracle 结果、快速路径复制结果
落盘为 `evidence/checksum_oracle.json`。

用法:
  python3 实验/issue87-stage3-tile-integrity/checksum_oracle.py [--build build/linux-dev]
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(HERE, "checksum_oracle.cpp")
OUT_ROOT = os.path.join(HERE, "work", "checksum_oracle")
EVID = os.path.join(HERE, "evidence")

LIBS = [
    "libacsd_hips.a",
    "libacsd_hips_properties.a",
    "libacsd_aio.a",
    "libacsd_common.a",
    "libacsd_cfitsio.a",
]
INCLUDES = [
    "lib/infrastructure/aio/include",
    "lib/include",
    "lib/include/acsd",
    "lib/algorithms/shared",
]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default=os.path.join(REPO, "build", "linux-dev"))
    args = ap.parse_args()
    build = os.path.abspath(args.build)
    os.makedirs(EVID, exist_ok=True)
    exe = os.path.join(EVID, "checksum_oracle")
    cmd = ["g++", "-std=c++17", "-O2", "-o", exe, SRC]
    cmd += ["-I" + os.path.join(REPO, i) for i in INCLUDES]
    cmd += [os.path.join(build, l) for l in LIBS]
    cmd += ["-lz", "-lpthread", "-lm"]
    print("+ " + " ".join(cmd))
    rc = subprocess.run(cmd).returncode
    if rc != 0:
        print(f"compile failed rc={rc}", file=sys.stderr)
        return rc
    proc = subprocess.run([exe, OUT_ROOT], capture_output=True, text=True)
    sys.stdout.write(proc.stdout)
    if proc.stderr:
        sys.stderr.write(proc.stderr)
    cases = []
    for line in proc.stdout.splitlines():
        line = line.strip()
        if not line.startswith("{"):
            continue
        cases.append(json.loads(line))
    n_fail = sum(1 for c in cases if c.get("verdict") != "PASS")
    doc = {
        "schema": "acsd.issue87.checksum-oracle/v1",
        "oracle": "cfitsio fits_verify_chksum (ffgcks: raw on-disk header+data bytes)",
        "fast_path_replication": "raw header + fits_read_pix decoded typed pixels",
        "n_cases": len(cases),
        "n_fail": n_fail,
        "cases": cases,
    }
    path = os.path.join(EVID, "checksum_oracle.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(f"wrote {path}: n={len(cases)} fail={n_fail}")
    return 0 if (proc.returncode == 0 and n_fail == 0) else 1


if __name__ == "__main__":
    sys.exit(main())
