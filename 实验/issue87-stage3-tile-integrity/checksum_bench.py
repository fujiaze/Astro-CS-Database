#!/usr/bin/env python3
"""Issue #87 / E2：reader 层每 tile 开销微基准（修复前 vs 修复后）。

同一 checksum_probe.cpp 分别链接修复前/修复后的生产库，遍历真实 M42 writer 产品
MOC 声明的全部叶级 tile（523 个），量化 DATASUM/CHECKSUM 校验与错误分类的每 tile
开销（不含 export 编排与共享缓存的抖动）。输出 evidence/checksum_bench.json。

用法:
  python3 实验/issue87-stage3-tile-integrity/checksum_bench.py \
      --baseline /workspace/.e87-base-build --fixed build/linux-dev
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
EVID = os.path.join(HERE, "evidence")
PROBE = os.path.join(HERE, "checksum_probe.cpp")
HIPS = os.path.join(HERE, "work", "real", "hips")
LIBS = ["libacsd_hips.a", "libacsd_aio.a", "libacsd_cfitsio.a", "libacsd_common.a"]
INCLUDES = ["lib/infrastructure/aio/include",
            "lib/infrastructure/aio/third_party/cfitsio"]


def build_and_run(build: str, tag: str) -> dict:
    build = os.path.abspath(build)
    exe = os.path.join(EVID, f"checksum_probe_{tag}")
    cmd = ["g++", "-std=c++17", "-O2", "-o", exe, PROBE]
    cmd += ["-I" + os.path.join(REPO, i) for i in INCLUDES]
    cmd += [os.path.join(build, l) for l in LIBS]
    cmd += ["-lz", "-lpthread", "-lm"]
    rc = subprocess.run(cmd).returncode
    if rc != 0:
        raise SystemExit(f"compile {tag} failed rc={rc}")
    out = subprocess.run([exe, HIPS], capture_output=True, text=True)
    lines = [l for l in out.stdout.strip().splitlines() if l]
    per = [float(m.group(1)) for l in lines
           if (m := re.search(r"ns_per_tile=(\d+)", l))]
    best = float(re.search(r"best_ns_per_tile=(\d+)", out.stdout).group(1))
    return {"tag": tag, "build": build, "exe": exe,
            "stdout": out.stdout, "runs_ns_per_tile": per,
            "best_ns_per_tile": best, "returncode": out.returncode,
            "acsd_lib_sha256": hashlib.sha256(
                open(os.path.join(build, "libacsd_hips.a"), "rb").read()).hexdigest()}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--baseline", required=True)
    ap.add_argument("--fixed", required=True)
    args = ap.parse_args()
    os.makedirs(EVID, exist_ok=True)
    res = {"baseline": build_and_run(args.baseline, "prefix"),
           "fixed": build_and_run(args.fixed, "fixed")}
    b = res["baseline"]["best_ns_per_tile"]
    f = res["fixed"]["best_ns_per_tile"]
    res["delta_ns_per_tile"] = f - b
    res["delta_pct"] = (f - b) / b * 100.0 if b else None
    res["tiles"] = 523
    json.dump(res, open(os.path.join(EVID, "checksum_bench.json"), "w",
                        encoding="utf-8"), indent=2)
    print(f"baseline {b:.0f} ns/tile, fixed {f:.0f} ns/tile, "
          f"delta {f-b:+.0f} ns/tile ({res['delta_pct']:+.1f}%)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
